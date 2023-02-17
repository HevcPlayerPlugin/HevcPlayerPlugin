class webSocketClient {
    constructor(port, canvas) {
        this.resolutionArray = [  //分辨率
            [0, 0],
            [256, 144],
            [640, 360],
            [800, 600],
            [1280, 720],
            [1920, 1080]
        ];
        this.sampleRateArray = [
            8000,
            12000,
            16000,
            32000,
            44100,
            48000,
            96000
        ];
        this.sampleFormatArray = [
            'none',
            'Int8',
            'Int16',
            'Int32',
            'Float32'
        ];
        this.port = port;

        this.canvas = canvas;
        this.ws = null;
        this.pcmPlayer = null;
        this.yuvPlayer = null;
        this.avRawHeaderSize = 8;
        this.width = 1280;
        this.height = 720;
        this.channels = 1;
        this.sampleRate = 44100;
        this.sampleFormatIndex = 4; // Float32

        this.checkInit();
    }

    initYuvPlayer(w, h, canvas) {
        canvas.width = w;
        canvas.height = h;
        this.yuvPlayer = new WebglScreen(canvas);
    }
    initPcmPlayer(c, s) {
        this.pcmPlayer = new PCMPlayer({
            inputCodec: "Float32",
            channels: c,
            sampleRate: s,
            flushTime: 100
        });
    }

    initWebsocket() {
        var socketURL = 'ws://localhost:' + this.port;
        this.ws = new WebSocket(socketURL);
        this.ws.binaryType = 'arraybuffer';

        let that = this;

        this.ws.onopen = function (event) {
            that.showToast("connect port " + that.port + " success.");
        };
        this.ws.onmessage = function (event) {
            that.onMessage(event);
        }
        this.ws.onclose = function (event) {
            if (that.pcmPlayer) {
                that.pcmPlayer.destroy();
            }
            if (that.yuvPlayer) {
                that.yuvPlayer.destroy();
            }
            that.ws = null;
            that.showToast("Connection Closed.");
        }
        this.ws.onerror = function (event) {
            if (that.pcmPlayer) {
                that.pcmPlayer.destroy();
            }
            if (that.yuvPlayer) {
                that.yuvPlayer.destroy();
            }
            that.ws = null;
            that.showToast("Connection Closed.");
        }
    }

    checkInit() {
        let that = this;
        if (that.yuvPlayer == null) {
            that.initYuvPlayer(1280, 720, that.canvas);
        }
        if (that.pcmPlayer == null) {
            that.initPcmPlayer(1, 44100);
        }
        if (that.ws == null) {
            that.initWebsocket();
        }
    }

    onMessage(event) {
        let that = this;
        let data = event.data;
        if (typeof data !== 'string') {
            let header = new Uint8Array(data, 0, that.avRawHeaderSize);
            // video-yuv420P
            if (header[0] == 1) {
                if (that.yuvPlayer == null) {
                    return;
                }

                let type = header[1];
                if (type >= 0) {
                    let w = that.resolutionArray[type + 1][0];
                    let h = that.resolutionArray[type + 1][1];
                    if (that.width != w || that.height != h) {
                        that.yuvPlayer.setSize(w, h, w);
                        that.width = w;
                        that.height = h;
                    }
                    that.yuvPlayer.renderImg(w, h,
                        new Uint8Array(data, that.avRawHeaderSize, w * h * 3 / 2));
                }
            }
            // audio-pcm
            else if (header[0] == 0) {
                if (that.pcmPlayer == null) {
                    return;
                }
                
                let f = header[1];
                let c = header[2];
                let s = that.sampleRateArray[header[3]];
                if (that.pcmPlayer.option.channels != c
                    || that.pcmPlayer.option.sampleRate != s
                    || that.pcmPlayer.option.inputCodec != that.sampleFormatArray[f]) {
                    that.pcmPlayer.destroy();
                    that.pcmPlayer = null;
                    that.pcmPlayer = new PCMPlayer({
                        inputCodec: that.sampleFormatArray[f],
                        channels: c,
                        sampleRate: s,
                        flushTime: 100
                    });
                    that.channels = c;
                    that.sampleRate = s;
                    that.inputCodec = that.sampleFormatArray[f];
                }
                that.pcmPlayer.feed(data.slice(that.avRawHeaderSize, data.byteLength));
            }
        } else {
            const payload = JSON.parse(data);
            if (payload.type == 5) {
                that.showToast("version: " + payload.version);
            }
            if (payload.result != 0) {
                that.showToast("[" + payload.result + "]" + payload.message);
            }
        }
    }

    doSendMessage(msg) {
        if (this.ws.readyState === 1) {
            this.ws.send(msg);
        } else {
            this.showToast("Websocket NOT connected. readyState: " + ws.readyState);
        }
    }

    /* Player controls Start Here */
    doPlay(mediaUrl, gpu) {
        this.checkInit();

        var dataJson = {
            "type": 1,
            "param": {
                "url": mediaUrl,
                "use_gpu": gpu,
                "width": this.width,
                "height": this.height
            }
        }
        this.doSendMessage(JSON.stringify(dataJson));
    }
    doStop() {
        let that = this;
        var dataJson = {
            "type": 2,
        }
        that.doSendMessage(JSON.stringify(dataJson));

        if (that.pcmPlayer) {
            that.pcmPlayer.destroy();
            that.pcmPlayer = null;
        }
        if (that.yuvPlayer) {
            that.yuvPlayer.destroy();
            that.yuvPlayer = null;
        }
    }
    doChangeVolume(percent) {
        this.pcmPlayer.volume(percent);
    }
    doChangeResolution(value) {
        this.checkInit();

        var dataJson = {
            "type": 3,
            "param": {
                "width": this.resolutionArray[value][0],
                "height": this.resolutionArray[value][1]
            }
        }
        this.doSendMessage(JSON.stringify(dataJson));
    }
    doDiscardFrames(value) {
        this.checkInit();

        var dataJson = {
            "type": 4,
            "param": {
                "enabled": value
            }
        }
        this.doSendMessage(JSON.stringify(dataJson));
    }
    doGetVersion() {
        this.checkInit();

        var dataJson = {
            "type": 5,
        }
        this.doSendMessage(JSON.stringify(dataJson));
    }
    showToast(message) {
        // 先清空现有toast
        var elements = document.getElementsByClassName('toast');
        for (var i = 0; i < elements.length; i++) {
            var element = elements[i];
            element.parentNode.removeChild(element);
        }

        // 创建toast消息的div
        var toast = document.createElement('div');
        toast.className = 'toast';
        toast.innerHTML = message;

        // 添加到页面中
        document.body.appendChild(toast);

        // 3秒后移除toast消息
        setTimeout(function () {
            document.body.removeChild(toast);
        }, 3000);
    }
    destroy() {
        let that = this;
        that.ws.close();
        that.ws = null;

        that.yuvPlayer.destroy();
        that.yuvPlayer = null;

        that.pcmPlayer.destroy();
        that.pcmPlayer = null;
    }
}

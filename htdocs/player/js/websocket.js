class webSocketClient {
    constructor(port, canvas, index, callback) {
        this.resolutionArray = [  //分辨率
            [0, 0],
            [256, 144],
            [640, 360],
            [800, 600],
            [1280, 720],
            [1920, 1080]
        ];
        this.index = index;
        this.port = port;
        this.canvas = canvas;
        this.callback = callback;
        this.ws = null;
        this.yuvPlayer = null;
        this.avRawHeaderSize = 8;

        this.checkInit();
    }

    initYuvPlayer(w, h, canvas) {
        canvas.width = w;
        canvas.height = h;
        this.yuvPlayer = new WebglScreen2D(canvas);
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
            if (that.yuvPlayer) {
                that.yuvPlayer.destroy();
            }
            that.ws = null;
            that.showToast("Connection Closed.");
        }
        this.ws.onerror = function (event) {
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
            that.initYuvPlayer(1, 1, that.canvas);
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
            let w = (header[0] << 8) + header[1];
            let h = (header[2] << 8) + header[3];
            let ts = (header[4] << 24) + (header[5] << 16) + (header[6] << 8) + header[7];
            if (that.callback) {
                that.callback(that.index, w, h, ts);
            }
            // video-yuv420P
            if (that.yuvPlayer == null) {
                return;
            }
            that.yuvPlayer.renderImg(w, h, new Uint8Array(data, that.avRawHeaderSize, w * h * 3 / 2));
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
        if (this.ws && this.ws.readyState === 1) {
            this.ws.send(msg);
            console.log(msg);
        } else {
            this.showToast("Websocket NOT connected");
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
                "width": 1,
                "height": 1
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

        if (that.yuvPlayer) {
            that.yuvPlayer.destroy();
            that.yuvPlayer = null;
        }
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
    }
}

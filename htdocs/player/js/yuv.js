https://developer.mozilla.org/en-US/docs/Web/API/CanvasRenderingContext2D/drawImage
class WebglScreen2D {
    constructor(canvas) {
        this.canvas = canvas;
        this.gl = canvas.getContext('2d')
    }
    renderImg(width, height, data) {
        this.setSize(width, height, width);
        // https://developer.mozilla.org/en-US/docs/Web/API/VideoFrame/format
        const init = {timestamp: 0, codedWidth: width, codedHeight: height, format: 'NV12'};
        let videoFrame = new VideoFrame(data, init)
        let gl = this.gl;
        gl.drawImage(videoFrame, 0, 0, width, height);
        videoFrame.close();
    }
    setSize(width, height) {
        this.canvas.width = width;
        this.canvas.height = height;
    }
    destroy() {
        var w = this.canvas.width;
        var h = this.canvas.height;  
        this.gl.clearRect(0, 0, w, h);
    }
}
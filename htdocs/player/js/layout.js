class UILayout {
    static SetScreenNumber(num) { UILayout.ScreenNumber = num; } 
    static GetScreenNumber() { return UILayout.ScreenNumber; } 
    static SetContainer(id) { UILayout.Container = document.getElementById(id); } 
    static GetContainer() { return UILayout.Container; } 
    static SetVideoWith(VideoWidth) { UILayout.VideoWidth =  VideoWidth; } 
    static GetVideoWith() { return UILayout.VideoWidth; } 
    static SetVideoHeight(VideoHeight) { UILayout.VideoHeight = VideoHeight; } 
    static GetVideoHeight() { return UILayout.VideoHeight; } 
    static SetSelectVideoIndex(index) { UILayout.SelectVideoIndex = index; } 
    static GetSelectVideoIndex() { return UILayout.SelectVideoIndex; } 

    static Init(id, screenNums, maxScreenNums) {
        UILayout.SetContainer(id);
        UILayout.SetScreenNumber(screenNums); 
        //UILayout.SetVideoWith(352);
        //UILayout.SetVideoHeight(288); 
        UILayout.SetSelectVideoIndex(-1);
        UILayout.CreateCanvas(maxScreenNums); 
        UILayout.LayoutScreens(screenNums);
    } 

    static CreateCanvas(maxScreenNums) {
        for (var i = 1; i <= maxScreenNums; i++) { //显示画布 
            var canvas = document.createElement('canvas'); 
            // canvas.width = UILayout.GetVideoWith();
            // canvas.height = UILayout.GetVideoHeight(); 
            canvas.style.border = "1px solid black"; 
            canvas.style.cssFloat = "left"; 
            this.Container.append(canvas); 
            // var ctx  = canvas.getContext('2d'); 
            // ctx.fillStyle = "gray"; 
            // ctx.fillRect(1, 1, canvas.width, canvas.height);
        }
    } 
    static ContainsScreen(num) {
        var screens = [1, 4, 9, 16]; 
        for (var i = 0; i < screens.length; i++) {
            if (screens[i] == num) { 
                return true; 
            }
        } 
        return false;
    } 
    static LayoutScreens(num) {
        if (num  == undefined) { 
            console.log("LayoutScreens num is undefined"); 
        } else if (!UILayout.ContainsScreen(num)) {
            console.log("LayoutScreens num is not in [1, 4, 9, 16]"); 
            return; 
        } else { 
            this.ScreenNumber = num; 
        } 
        for (var i = 1; i <= this.Container.childElementCount; i++) {
            var video = this.Container.childNodes.item(i); 
            video.index = i; 
            video.style.margin = "1px";
            video.parentContainer = this.Container; 
            video.onclick = function () {
                UILayout.SelectVideoIndex = this.index; 
                //alert(UILayout.SelectVideoIndex); 
                for (var i = 1; i <= this.parentContainer.childElementCount; i++) {
                    if (i === UILayout.SelectVideoIndex) { 
                        this.style.border = "1px solid #00FF00"; 
                    } else {
                        this.parentContainer.childNodes.item(i).style.border = "1px solid black";
                    }
                }
            }; 
            if (this.ScreenNumber < i) { 
                video.style.display = "none"; 
            } else {
                video.style.display = "block";
            }
        } 
        var width = parseInt(this.Container.parentElement.clientWidth); 
        var height = parseInt(this.Container.parentElement.clientHeight); 
        var count = 0; 
        for (var i = 1; i <= this.Container.childElementCount; i++) {
            var video = this.Container.childNodes.item(i); 
            if (this.ScreenNumber == 1 && video.index == 1) {
                video.style.width = (width - 5) + "px"; 
                video.style.height = (height - 5) + "px"; 
                count++;
            } else if (this.ScreenNumber == 4 && video.index <= 4) {
                video.style.width = (width / 2 - 5) + "px"; 
                video.style.height = (height / 2 - 5) + "px"; 
                count++;
            } else if (this.ScreenNumber == 9 && video.index <= 9) {
                video.style.width = (width / 3 - 5) + "px"; 
                video.style.height = (height / 3 - 5) + "px"; 
                count++;
            } else if (this.ScreenNumber == 16 && video.index <= 16) {
                video.style.width = (width / 4 - 5) + "px"; 
                video.style.height = (height / 4 - 5) + "px"; 
                video.style.cssFloat = "left"; 
                count++;
            }
            
            if (count == this.ScreenNumber) { 
                break; 
            }
        }
    }
} 
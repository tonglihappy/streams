package stream

//#cgo LDFLAGS: -L../lib/stream/cgo/lib -lffmpeg
//#cgo LDFLAGS: -L./cgo/lib -lavformat -lavcodec -lswscale -lavutil -lavfilter -lswresample -lavdevice -lpostproc -lx264 -ldl -lm -lrt -lpthread -lstdc++
//#cgo CFLAGS: -I ./cgo/include
//#include "main_publish.h"
//#include "main_pull.h"
import "C"

import (
	"fmt"
	"time"
)

func PushStream(filename string, url string) {
	cf := C.CString(filename)
	curl := C.CString(url)

	C.push_stream(cf, curl)
	//fmt.Println(i)
}

func PullStream(url string, infi int) int {
	return int(C.pull_stream(C.CString(url), C.int(infi)))
}

func KillStream() {
	C.kill_all_stream()
}

func ThreadKill() {
	//	C.thread_kill()
}

func main() {
	for i := 0; i < 10; i++ {
		go PushStream("/root/livecdn_autotest/src/media/ss.flv", "rtmp://ip/domain/live/test011")
		time.Sleep(2 * time.Second)
		ret := PullStream("rtmp://ip/domain/live/test011", 0)
		if ret == 200 {
			fmt.Println("pull stream success")
		} else {
			fmt.Println("pull stream failed")
		}
		KillStream()
		time.Sleep(1 * time.Second)
	}
}

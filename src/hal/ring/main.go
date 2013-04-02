package main

import "fmt"
import "math/rand"
import "time"
import "ring"

var done = make(chan int)

func ping(ring * ring.Ring, count int) {
	for i := 0; i != count; i++ {
		pad := rand.Intn(100);
		//fmt.Println("ping", i)
		for ring.WriteString(fmt.Sprintf("data %.*d", pad, i)) != nil {}
	}
	for ring.WriteString("") != nil {}
}

func pong(ring * ring.Ring) {
	count, bytes := 0, 0
	begin := time.Now()
	for {
		data := ring.NextString()
		if data == nil { continue }

		count++
		bytes += len(*data)
		//fmt.Println("Pong", count, bytes, *data)
		if len(*data) == 0 { break }
	}
	t := time.Now().Sub(begin)
	fmt.Printf("Messages: %d; Bytes: %d; Time: %v\n", count, bytes, t)
	fmt.Printf("Speed: %.3fm/us\n", float64(count) / (1000000 * t.Seconds()))
	done <- 1
}

func main() {
	ring := ring.New(1024)
	ring.WriteString("test")
	ring.WriteString("")
	fmt.Println(ring)
	fmt.Println(ring.NextString())
	fmt.Println(ring.NextString())
	fmt.Println(ring.NextString())
	ring.WriteString("xxx")
	fmt.Println(ring.NextString())
	go ping(ring, 1000000)
	go pong(ring)
	<-done
	/*
	for i := 0; true; i++ {
		//fmt.Println("data", i)
		pad := rand.Intn(10);
		err := ring.WriteString(fmt.Sprintf("data %.*d", pad, i))
		if err != nil {
			fmt.Println("Error at ", i, err)
			break
		}
	}
	*/
}

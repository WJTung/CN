# CN 2016 HW2 - Implement Multipath TCP with Congestion Control by UDP

## Introduction

In this project, we have to implement TCP with congestion control by UDP. There are three components in the project :
* Sender
  * Send file by UDP
  * Congestion Control
    * Slow start
      * Send single packet in the beginning
      * When *below* the threshold, congestion window increase exponentially until packet loss
      * When *larger than or equal to* the threshold, congestion window increase linearly until packet loss
    * Packet loss
      * Adjust the threshold
      * Set Congestion window to 1
      * Retransmit
* Receiver
  * Receive file by UDP
  * Buffer Handling
    * Buffer Overflow : Drop packet if out of range of buffer
    * Flush (write) to the file only when buffer overflows and all packets in range are received
* Agent
  * Forward data and ACK packets
  * Randomly drop data packet
  * Compute loss rate

## Usage

**Agent**  
	
    ./agent AGENT_SENDER_PORT AGENT_RECEIVER_PORT drop rate
  
**Receiver**  
	
    ./receiver AGENT_IP AGENT_RECEIVER_PORT destination file
 
**Sender**  
	
    ./sender AGENT_IP AGENT_SENDER_PORT source file
    
## Report
**Please refer to** [report.pdf](https://github.com/WJTung/CN/blob/master/2/b03902062_report.pdf) **for more details**

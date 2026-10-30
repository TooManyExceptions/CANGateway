This project aims to act as a gateway between CAN 2.0A and 2.0B devices. It uses the [mcp_can](https://github.com/coryjfowler/MCP_CAN_lib) library.

# CAN Buses
This hasn't been tested yet but should receive CAN frames on two buses (one for devices which only support standard IDs and one for extended IDs).\
Then once a frame has been received:
- if it has an extended ID: the ID gets truncated to 11 bits and is sent on the other bus
- if it has a standard ID: the ID is sent, as is, to the other bus

# Implementation:
It uses interrupt service routine procedures for each buses interrupt pin (activated on LOW), if the interrupt is triggered the frame is read from the bus and added to a queue (without further translation). Then in the main loop procedure, if there are any frames in this queue, they are sent to the other bus and translated if necessary.
If you'd like to change the translation to something else, such as a table lookup, you can change the `convertToStandard` function.

# Considerations:
- If the frame queue is full when receiving a new frame, the oldest frame (which hasn't been sent) will be overridden. This also means that you don't need to check if the queue is full before enqueueing.
- The main loop checks if there are any frames which haven't been sent yet before dequeueing, meaning the dequeue method doesn't check this.
- It shouldn't matter if an interrupt happens in the middle of the main loop, because interrupts only add new frames.
- The Arduino only has one core, meaning it should be okay having a shared queue length variable.
- The queue structure has to be volatile, because the queue is shared between the interrupt methods and the main loop.
- The interrupts shouldn't interrupt each other.

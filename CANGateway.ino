#include <mcp_can.h>
#include <SPI.h>

#define STANDARD_BUS_CHIP_SELECT_PIN   8
#define STANDARD_BUS_INTERRUPT_PIN     2
#define EXTENDED_BUS_CHIP_SELECT_PIN   9
#define EXTENDED_BUS_INTERRUPT_PIN     3
#define INTERRUPT_TRIGGER            LOW

#define MAX_FRAMES                    64 // 1 to 256 (because length is uint8_t)

// A way to bundle the raw information back from mcp_can library.
struct Frame
{
  uint32_t id_with_flags;
  uint8_t dataLength;
  uint8_t data[8];
};

struct FrameQueue
{
  Frame queue[MAX_FRAMES];
  uint8_t headPointer = 0; // Next available place to dequeue (contains item)
  uint8_t tailPointer = 0; // Next available place to enqueue (doesn't contain item)

  uint8_t length;

  bool empty()
  {
    return length <= 0;
  }
  bool full()
  {
    return length >= MAX_FRAMES;
  }

  // Returns a pointer to the next possible place to enqueue a frame.
  Frame* enqueue()
  {
    Frame* pointer = &queue[tailPointer];
    
    tailPointer = ((tailPointer + 1) % MAX_FRAMES);

    // Only increment length when the queue is not full, because otherwise
    // elements are overriden (meaning length doesn't increase).
    if (length < MAX_FRAMES) length += 1;

    return pointer;
  }

  // Returns a pointer to the next item to dequeue
  Frame* dequeue()
  {
    Frame* pointer = &queue[headPointer];

    headPointer = ((headPointer + 1) % MAX_FRAMES);
    length -= 1;

    return pointer;
  }
};

MCP_CAN CanWithInterupt(int bus_pin, int interupt_pin, void (*isr)())
{
  attachInterrupt(digitalPinToInterrupt(interupt_pin), isr, INTERRUPT_TRIGGER);
  return new MCP_CAN(bus_pin);
}

volatile MCP_CAN standard = CanWithInterupt(STANDARD_BUS_CHIP_SELECT_PIN, STANDARD_BUS_INTERRUPT_PIN, recieveStandard);
volatile MCP_CAN extended = CanWithInterupt(EXTENDED_BUS_CHIP_SELECT_PIN, EXTENDED_BUS_INTERRUPT_PIN, recieveExtended);

volatile FrameQueue queue;

void recieveStandard()
{
  recieve(&standard);
}
void recieveExtended()
{
  recieve(&extended);
}
void recieve(MCP_CAN* bus)
{
  Frame* frame = queue.enqueue();
  
  bus->readMsgBuf(&frame->id_with_flags, &frame->dataLength, (uint8_t*) &frame->data);
}

uint32_t convertToStandard(uint32_t id_with_flags)
{
  return id_with_flags & ((1 << 11) - 1); // Truncates all bits except the 11 bits at the beginning
}

void setup()
{
	standard.setMode(MCP_NORMAL);   // Change to normal mode to allow messages to be transmitted
	extended.setMode(MCP_NORMAL);   // Change to normal mode to allow messages to be transmitted
}

void loop()
{
  while (!queue.empty())
  {
    Frame* frame = queue.dequeue();
    uint32_t id_with_flags = frame->id_with_flags;

    if ((id_with_flags & 0x80000000) == 0x80000000)
    {
      // Extended frame, so translate and forward
      if (standard.sendMsgBuf(convertToStandard(id_with_flags), 0, frame->dataLength, frame->data) == CAN_OK)
      {
        Serial.println("Frame sent successfully");
      } else
      {
        Serial.println("Error sending frame");
      }
    } else
    {
      // Standard frame, so just forward
      if (extended.sendMsgBuf(id_with_flags, frame->dataLength, frame->data) == CAN_OK)
      {
        Serial.println("Frame sent successfully");
      } else
      {
        Serial.println("Error sending frame");
      }
    }
  }
}

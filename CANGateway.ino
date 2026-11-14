#include <mcp_can.h>
#include <SPI.h>

#define STANDARD_BUS_CHIP_SELECT_PIN       8
#define STANDARD_BUS_INTERRUPT_PIN         2
#define EXTENDED_BUS_CHIP_SELECT_PIN       9
#define EXTENDED_BUS_INTERRUPT_PIN         3
#define INTERRUPT_TRIGGER            FALLING

#define MAX_FRAMES                        64 // 1 to 256 (because length is uint8_t)


enum BusSymbol
{
  Extended,
  Standard
};

// A way to bundle the raw information back from mcp_can library.
struct Frame
{
  uint32_t id_with_flags;
  uint8_t dataLength;
  uint8_t data[8];
  BusSymbol busSentOn;

  void display()
  {
    Serial.print("Sent on ");
    switch (busSentOn)
    {
      case Standard:
        Serial.print("standard ");
        break;
      case Extended:
        Serial.print("extended ");
        break;
    }
    Serial.println("bus");
    
    Serial.print("ID type: ");
    if ((id_with_flags & 0x80000000) == 0x80000000)
    {
      // Extended frame

      Serial.println("extended");
    } else
    {
      // Standard frame
      Serial.println("standard");
    }

    Serial.print("Frame Type: ");
    if ((id_with_flags & 0x40000000) == 0x40000000)
    {
      Serial.println("remote");
    } else
    {
      Serial.println("data");
      Serial.print("Data:");
        
      char dataStr[128];

      for (byte i = 0; i < dataLength; i++)
      {
        sprintf(dataStr, " 0x%.2X", data[i]);
        Serial.print(dataStr);
      }
      Serial.println();
    }

    Serial.print("ID: ");
    Serial.println(id_with_flags & ((1UL << 29) - 1));
    
    Serial.print("ID with flags: ");
    Serial.println(id_with_flags);

    Serial.print("Data Length: ");
    Serial.println(dataLength);
  }
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

void startCanWithInterupt(MCP_CAN* bus, int interupt_pin, void (*isr)())
{
  Serial.println("About to add interupt...");
  
  int interupt_pin_result = digitalPinToInterrupt(interupt_pin);
  attachInterrupt(interupt_pin_result, isr, INTERRUPT_TRIGGER);
  SPI.usingInterrupt(interupt_pin_result);
  Serial.println("Interupt added");
  
  Serial.println("Beginning bus...");
  if (bus->begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK)
  {
    Serial.print("Initialised Successfully with:\n\tINT Pin: ");
    Serial.println(interupt_pin);
    
    Serial.println("Setting up bus mode...");
    bus->setMode(MCP_NORMAL);
    Serial.println("Bus mode set");
  } else
  {
    Serial.println("Error Initialising");
  }
}

volatile MCP_CAN standard = MCP_CAN(STANDARD_BUS_CHIP_SELECT_PIN);
volatile MCP_CAN extended = MCP_CAN(EXTENDED_BUS_CHIP_SELECT_PIN);

volatile FrameQueue queue;

void recieveStandard()
{
  Serial.println("Standard recieved");
  recieve(&standard, Standard);
}
void recieveExtended()
{
  Serial.println("Extended recieved");
  recieve(&extended, Extended);
}
void recieve(MCP_CAN* bus, BusSymbol busSentOn)
{
  Frame* frame = queue.enqueue();
  
  bus->readMsgBuf(&frame->id_with_flags, &frame->dataLength, (uint8_t*) &frame->data);
  frame->busSentOn = busSentOn;
}

void serialDebugFrame(int8_t sendMsgBufReturn)
{
  if (sendMsgBufReturn == CAN_OK)
  {
    Serial.println("Frame sent successfully");
  } else
  {
    Serial.println("Error sending frame");
  }
}

uint32_t convertToStandard(uint32_t id_with_flags)
{
  return id_with_flags & (((1UL << 11) - 1) | 0x40000000); // Truncates all bits except the 11 bits at the beginning, and keeps remote frame bit.
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) {}
  
  Serial.println("Serial Started");
  
  startCanWithInterupt(&standard, STANDARD_BUS_INTERRUPT_PIN, recieveStandard);
  startCanWithInterupt(&extended, EXTENDED_BUS_INTERRUPT_PIN, recieveExtended);
}

void loop()
{
  while (!queue.empty())
  {
    Frame* frame = queue.dequeue();
    frame->display();
    uint32_t id_with_flags = frame->id_with_flags;

    if ((id_with_flags & 0x80000000) == 0x80000000)
    {
      // Extended frame recieved:
      switch (frame->busSentOn)
      {
        case Extended:
          // Translate and forward to standard bus
          serialDebugFrame(standard.sendMsgBuf(convertToStandard(id_with_flags), frame->dataLength, frame->data));
          break;
        case Standard:
          // Forward to the extended bus
          serialDebugFrame(extended.sendMsgBuf(id_with_flags, frame->dataLength, frame->data));
          break;
      }
    } else
    { 
      // Standard frame, so forward to correct bus
      switch (frame->busSentOn)
      {
        case Extended:
          // Forward to standard bus
          serialDebugFrame(standard.sendMsgBuf(id_with_flags, frame->dataLength, frame->data));
          break;
        case Standard:
          // Forward to the extended bus
          serialDebugFrame(extended.sendMsgBuf(id_with_flags, frame->dataLength, frame->data));
          break;
      }
    }
  }
}

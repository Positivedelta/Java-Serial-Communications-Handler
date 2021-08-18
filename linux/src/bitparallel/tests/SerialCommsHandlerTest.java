package bitparallel.tests;

//
// (c) Bit Parallel Ltd, December 2020
//

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.io.IOException;

import bitparallel.communication.SerialByteBufferListener;
import bitparallel.communication.SerialCommsHandler;

public class SerialCommsHandlerTest implements SerialByteBufferListener
{
    public SerialCommsHandlerTest(final String device) throws IOException
    {
        final SerialCommsHandler handler = new SerialCommsHandler();
        handler.addSerialByteBufferListener(this);
        handler.start(device, 57600);

        // note, if testing with a linux client, make sure that the baud rate, no local echo and raw mode are configured
        //       raw mode will allow the transmitted bytes/chars to be received immediately
        //
        // use: stty -F /dev/ttyUSB0 57600 -echo raw
        // [rx] cat < /dev/ttyUSB0
        // [tx] echo -n "My name is Max van Daalen and I live in Bracknell, Berkshire, UK. For tea I am planing to have a chicken sandwich, yum yum!" > /dev/ttyUSB0
        //
        final String message = "Hello world!";
        final byte[] ascii = message.getBytes(StandardCharsets.US_ASCII);
        handler.transmit(ascii);

        final Thread t = new Thread(() -> {
            try
            {
                Thread.sleep(15000);
                handler.stop();
            }
            catch (Exception ex)
            {
                ex.printStackTrace();
            }
        });

        t.start();
    }

    public final void rxedSerialByteBuffer(final ByteBuffer buffer)
    {
        // don't actually need to copy the data, although the array() method is optional, so playing safe
        //
        final byte[] bytes = new byte[buffer.limit()];
        buffer.get(bytes);

        System.out.println("RXed data: [" + (new String(bytes)) + "], " + bytes.length + " bytes");
    }

    public static final void main(String[] args) throws IOException
    {
        final String device = args[0];
        final SerialCommsHandlerTest test = new SerialCommsHandlerTest(device);
    }
}

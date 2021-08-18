package bitparallel.communication;

//
// (c) Bit Parallel Ltd, August 2021
//

import java.nio.ByteBuffer;

public interface SerialByteBufferListener
{
    public void rxedSerialByteBuffer(final ByteBuffer buffer);
}

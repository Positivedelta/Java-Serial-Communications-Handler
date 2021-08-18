package bitparallel.communication;

//
// (c) Bit Parallel Ltd, December 2020
//

import java.nio.ByteBuffer;

public interface SerialByteBufferListener
{
    public void serialRxedByteBuffer(final ByteBuffer buffer);
}

package bitparallel.communication;

//
// (c) Bit Parallel Ltd, December 2020
//

import java.nio.ByteBuffer;

public interface ByteBufferListener
{
    public void rxedByteBuffer(final ByteBuffer buffer);
}

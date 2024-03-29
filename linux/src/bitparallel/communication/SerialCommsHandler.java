package bitparallel.communication;

//
// (c) Bit Parallel Ltd, May 2023
//

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.concurrent.CopyOnWriteArrayList;

import org.apache.logging.log4j.Logger;
import org.apache.logging.log4j.LogManager;

public class SerialCommsHandler
{
    private static final Logger logger = LogManager.getLogger(SerialCommsHandler.class);

    static
    {
        // FIXME! may need to include "sun.arch.data.model", not sure...
        //
        final String osName = System.getProperty("os.name");
        final String osArch = System.getProperty("os.arch");
        if ("Linux".equals(osName))
        {
            if ("arm".equals(osArch))
            {
                loadNativeLibrary("libserial_comms_handler_linux_arm32.so");
            }
            else if ("aarch64".equals(osArch))
            {
                loadNativeLibrary("libserial_comms_handler_linux_arm64.so");
            }
            else if ("amd64".equals(osArch))
            {
                loadNativeLibrary("libserial_comms_handler_linux_amd64.so");
            }
        }
        else if (osName.contains("Windows") && "amd64".equals(osArch))
        {
            // FIXME! add 32 bit support? not really needed...
            //
            loadNativeLibrary("libserial_comms_handler_x64.dll");
        }
        else
        {
            final StringBuffer sb = new StringBuffer();
            sb.append("The detected OS and architecture combination is not supported: ");
            sb.append(osName);
            sb.append(" (");
            sb.append(osArch);
            sb.append(")");

            throw new UnsatisfiedLinkError(sb.toString());
        }
    }

    // note, these are also referenced within the native code
    //
    private static final int RX_BUFFER_SIZE = 4096;
    private static final int RX_SELECT_TIMEOUT_US = 100000;

    private String device;
    private long deviceFd;
    private final ByteBuffer rxBuffer;
    private boolean connected;
    private Thread rxCpu;
    private CopyOnWriteArrayList<SerialByteBufferListener> byteBufferListeners;

    public SerialCommsHandler()
    {
        // this buffer is written to and re-used by the nativeRxRead() method, so manage and use appropriately
        //
        rxBuffer = ByteBuffer.allocateDirect(RX_BUFFER_SIZE);

        deviceFd = -1;
        connected = false;
        byteBufferListeners = new CopyOnWriteArrayList<SerialByteBufferListener>();
    }

    private native long nativeStart(final String device, final int baudRate) throws IOException;
    private native void nativeTransmit(final byte[] bytes, final String device, final long deviceFd) throws IOException;
    private native void nativeStop(final String device, final long deviceFd) throws IOException;
    private native void nativeRxRead(final ByteBuffer rxBuffer, final String device, final long deviceFd) throws IOException;

    public final boolean start(final String device, final int baudRate) throws IOException
    {
        this.device = device;

        if (connected)
        {
            logger.error("Already connected to " + device + " at " + baudRate + " Baud");
            return false;
        }

        deviceFd = nativeStart(device, baudRate);
        logger.info("Successfully connected to " + device + " at " + baudRate + " Baud");
        connected = true;

        final Runnable rxTask = () -> {
            while (connected)
            {
                try
                {
                    // note, the native method will set the limit() to the number of returned bytes
                    //
                    rxBuffer.clear();
                    nativeRxRead(rxBuffer, device, deviceFd);
                    if (rxBuffer.limit() > 0) for(SerialByteBufferListener byteBufferListener : byteBufferListeners) byteBufferListener.rxedSerialByteBuffer(rxBuffer);
                }
                catch (final IOException ex)
                {
                    // FIXME! investigate ways to throw this exception out of the thread
                    //
                    logger.error("Unable to read " + device + ", reason: " + ex.getMessage(), ex);
                }
            }
        };

        rxCpu = new Thread(rxTask);
        rxCpu.setDaemon(true);
        rxCpu.start();

        return true;
    }

    public final boolean transmit(final byte[] bytes) throws IOException
    {
        if (!connected)
        {
            logger.error("Unable to transmit, not connected");
            return false;
        }

        nativeTransmit(bytes, device, deviceFd);
        return true;
    }

    public final boolean stop() throws IOException
    {
        if (!connected)
        {
            logger.error("Nothing to stop, not connected");
            return false;
        }

        connected = false;
        try
        {
            rxCpu.join();
        }
        catch (final InterruptedException ex)
        {
        }

        try
        {
            nativeStop(device, deviceFd);
        }
        catch (final IOException ex)
        {
            deviceFd = -1;
            throw ex;
        }

        return true;
    }

    public void addListener(final SerialByteBufferListener byteBufferListener)
    {
        byteBufferListeners.add(byteBufferListener);
    }

    public void removeListener(final SerialByteBufferListener byteBufferListener)
    {
        byteBufferListeners.remove(byteBufferListener);
    }

    public void clearListeners()
    {
        byteBufferListeners.clear();
    }

    // native library loading helper method, see the static initialiser above
    //
    private static final void loadNativeLibrary(final String libraryName) throws UnsatisfiedLinkError
    {
        try
        {
            final File javaTemp = new File(System.getProperty("java.io.tmpdir"));
            final File tempDir = new File(javaTemp, SerialCommsHandler.class.getName());
            if (!tempDir.isDirectory()) tempDir.mkdir();

            final File nativeLib = new File(tempDir, libraryName);
            final InputStream is = SerialCommsHandler.class.getClassLoader().getResourceAsStream(libraryName);
            Files.copy(is, nativeLib.toPath(), StandardCopyOption.REPLACE_EXISTING);
            is.close();

            // note, if using loadLibrary() on linux the actual file name must be prefixed with 'lib', it's a naming convention, but not on windows!
            //
            System.load(tempDir.getPath() + File.separator + libraryName);
        }
        catch (final Exception ex)
        {
            final StringBuffer sb = new StringBuffer();
            sb.append("Failed to install the native library, reason: ");
            sb.append(ex.getMessage());

            throw new UnsatisfiedLinkError(sb.toString());
        }
    }
}

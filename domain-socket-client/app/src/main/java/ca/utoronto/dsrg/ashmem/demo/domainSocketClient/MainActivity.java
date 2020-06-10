package ca.utoronto.dsrg.ashmem.demo.domainSocketClient;

import android.os.Bundle;
import android.view.View;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.io.FileDescriptor;
import java.lang.reflect.Constructor;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;

public class MainActivity extends AppCompatActivity {
    // Shared memory metadata populated by getSharedMemoryRegionNative()
    static long mRawSharedMemoryPtr = 0L;
    static int mRawSharedMemoryFd = 0;
    static FileDescriptor mSharedMemoryFd = null;

    static MappedByteBuffer mDirectMappedBuffer = null;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Get shared memory region from server via unix domain socket
        getSharedMemoryRegionNative();
    }

    public void readValueInNative(View view) {
        if (mRawSharedMemoryPtr != 0) {
            Toast.makeText(getApplicationContext(), "Read from JNI:" + readValueInSharedMemoryNative(),
                    Toast.LENGTH_SHORT).show();
        }
    }

    public void readValueInJava(View view) {
        if (mDirectMappedBuffer == null) {
            try {
                // Accessing hidden methods via reflection requires "ChickenHook" package which works for API 19-30
                // https://github.com/ChickenHook/RestrictionBypass
                final Class<?> directByteBufferClass = Class.forName("java.nio.DirectByteBuffer");
                final Constructor<?> directByteBufferConstructor = directByteBufferClass.getConstructor(int.class,
                        long.class, FileDescriptor.class, Runnable.class, boolean.class);
                mDirectMappedBuffer = (MappedByteBuffer)directByteBufferConstructor.newInstance(Long.BYTES,
                        mRawSharedMemoryPtr, mSharedMemoryFd, null, true);   // READ-ONLY Direct Mapped Buffer
                mDirectMappedBuffer.order(ByteOrder.LITTLE_ENDIAN);
                mDirectMappedBuffer.load();
            } catch (Exception e) {
                e.printStackTrace();
                return;
            }
        } else {
            mDirectMappedBuffer.rewind();
        }
        long sharedBufferValue = mDirectMappedBuffer.getLong();
        Toast.makeText(getApplicationContext(), "Read from direct mapped buffer:" + sharedBufferValue,
                Toast.LENGTH_SHORT).show();
    }


    public native void getSharedMemoryRegionNative();   // Get shared memory region via unix domain socket
    public native long readValueInSharedMemoryNative();
}

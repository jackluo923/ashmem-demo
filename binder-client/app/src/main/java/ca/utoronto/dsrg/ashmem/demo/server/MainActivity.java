package ca.utoronto.dsrg.ashmem.demo.server;

import androidx.appcompat.app.AppCompatActivity;

import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import java.io.FileDescriptor;
import java.lang.reflect.Constructor;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;


public class MainActivity extends AppCompatActivity implements ServiceConnection {
    private static final String TAG = "MainActivity";
    IShmFdSharingService shmFdSharingService;
    static boolean isShmFdSharingServiceInitialized = false;

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

        // Create an explicit intent to connect to share memory file descriptor sharing service
        Intent explicitIntent = new Intent("ca.utoronto.dsrg.ashmem.demo.server.ShmFdSharingService");
        explicitIntent.setPackage("ca.utoronto.dsrg.ashmem.demo.server");
        bindService(explicitIntent, this, BIND_AUTO_CREATE);
    }

    public void readValueInNative(View view) {
        while (!isShmFdSharingServiceInitialized) {}   // Busy wait until shared memory is ready
        Toast.makeText(getApplicationContext(), "Read from JNI:" + readValueInSharedMemoryNative(),
                Toast.LENGTH_SHORT).show();
    }

    public void readValueInJava(View view) {
        while (!isShmFdSharingServiceInitialized) {}   // Busy wait until shared memory is ready
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

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        Log.i(TAG, "Shared memory file descriptor sharing service connected");
        shmFdSharingService = IShmFdSharingService.Stub.asInterface(service);
        try {
            ParcelFileDescriptor parcelFileDescriptor = shmFdSharingService.GetSharedMemoryFd();
            mRawSharedMemoryFd = parcelFileDescriptor.getFd();
            mSharedMemoryFd = parcelFileDescriptor.getFileDescriptor();
            getSharedMemoryRegionNative();
            isShmFdSharingServiceInitialized = true;
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {

    }

    public native void getSharedMemoryRegionNative();   // Get shared memory region via unix
    public native long readValueInSharedMemoryNative();
}

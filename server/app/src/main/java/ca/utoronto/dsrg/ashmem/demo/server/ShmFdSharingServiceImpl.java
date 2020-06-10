package ca.utoronto.dsrg.ashmem.demo.server;

import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;

import java.io.IOException;

public class ShmFdSharingServiceImpl extends IShmFdSharingService.Stub {
    private static final String TAG = "ShmFdSharingServiceImpl";

    @Override
    public ParcelFileDescriptor GetSharedMemoryFd() throws RemoteException {
        // Busy wait until shared memory fd is ready (almost immediately after APP startup)
        while (MainActivity.mRawSharedMemoryFd == 0) { }
        try {
            Log.i(TAG, "Sending shared memory file descriptor across binder");
            return ParcelFileDescriptor.fromFd(MainActivity.mRawSharedMemoryFd);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }
}

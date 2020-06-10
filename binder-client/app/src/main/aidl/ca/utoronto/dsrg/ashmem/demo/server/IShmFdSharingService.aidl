// IShmFdSharingService.aidl
package ca.utoronto.dsrg.ashmem.demo.server;

// Declare any non-default types here with import statements
import android.os.ParcelFileDescriptor;

interface IShmFdSharingService {
    ParcelFileDescriptor GetSharedMemoryFd();
}

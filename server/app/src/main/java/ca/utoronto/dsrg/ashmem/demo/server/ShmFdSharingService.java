package ca.utoronto.dsrg.ashmem.demo.server;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import androidx.annotation.Nullable;

public class ShmFdSharingService extends Service {
    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return new ShmFdSharingServiceImpl();
    }
}

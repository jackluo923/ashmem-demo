package ca.utoronto.dsrg.ashmem.demo.client;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;

public class MainActivity extends AppCompatActivity {
    // sharedMemoryType: 1 -> ashmem 2 -> AHardwareBuffer
    int sharedMemoryType = 1;   // default ashmem
    // fdCommunicationType: 1 -> API < 26, 2 -> API >= 26
    int useNewAPI = 2;   // default use new API


    MappedByteBuffer ashmemBuffer = null;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Detect shared memory configurations
        final Switch useAshmemSwitch = findViewById(R.id.useAshmem);
        final Switch useNewAPISwitch = findViewById(R.id.useNewAPI);
        final Button readValueViaJavaButton = findViewById(R.id.readValueViaJava);
        useAshmemSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    sharedMemoryType = 1;
                    useNewAPISwitch.setClickable(true);
                    readValueViaJavaButton.setClickable(true);
                    readValueViaJavaButton.setAlpha(1f);
                } else {
                    // Shared memory type: AHardwareBuffer
                    sharedMemoryType = 2;
                    useNewAPISwitch.setClickable(true);
                    useNewAPISwitch.setText("Use API >= 26");
                    readValueViaJavaButton.setClickable(false);
                    readValueViaJavaButton.setAlpha(.5f);
                }
            }
        });
        useNewAPISwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    useNewAPI = 1;
                    useNewAPISwitch.setText("Use API >= 26");
                } else {
                    useNewAPI = 0;
                    useNewAPISwitch.setText("Use API < 26");
                }
            }
        });
    }

    public void readTimestampNative(View view) {
        Toast.makeText(getApplicationContext(),
                "Read from ashmem:" + ashmemReadTimestampNative(),
                Toast.LENGTH_SHORT).show();
    }

    public void readTimestampInJava(View view) {
        if (ashmemBuffer == null) {
            FileInputStream ashmemFIS = new FileInputStream(getAshmemJavaFd());
            FileChannel fc = ashmemFIS.getChannel();
            try {
                ashmemBuffer = fc.map(FileChannel.MapMode.READ_ONLY, 0, 8);
                ashmemBuffer.order(ByteOrder.LITTLE_ENDIAN);
                Toast.makeText(getApplicationContext(),
                        "Read from ashmem:" + ashmemBuffer.getLong(0),
                        Toast.LENGTH_SHORT).show();
            } catch (IOException e) {
                e.printStackTrace();
            }
        } else {
            ashmemBuffer.rewind();
            Toast.makeText(getApplicationContext(),
                    "Read from ashmem 2nd time:" + ashmemBuffer.getLong(0),
                    Toast.LENGTH_SHORT).show();
        }
    }

    public native String ashmemReadTimestampNative();
    public native FileDescriptor getAshmemJavaFd();
}

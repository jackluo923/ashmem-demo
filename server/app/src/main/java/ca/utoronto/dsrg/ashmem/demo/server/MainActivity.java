package ca.utoronto.dsrg.ashmem.demo.server;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.Switch;
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

    boolean isDomainSocketServerStated = false;

    long shared_memory_value = 9000;
    MappedByteBuffer ashmemBuffer = null;
    int ashmemFd = -1;
    Thread domainSocketServer = null;

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

        // Initialize domain socket server switch
        final Switch domainSocketSwitch = findViewById(R.id.domainSocketServer);
        domainSocketSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    domainSocketServer = new Thread() {
                        public void run() {
                            ashmemServer();
                        }
                    };
                    domainSocketServer.start();
                    domainSocketSwitch.setText("On");
                    isDomainSocketServerStated = true;
                    domainSocketSwitch.setClickable(false);
                    useAshmemSwitch.setClickable(false);
                    useNewAPISwitch.setClickable(false);
                } else {
                    domainSocketSwitch.setText("Off");
                }
            }
        });
    }

    public void writeValueInNative(View view) {
        if(isDomainSocketServerStated == true) {
            EditText shared_memory_value_str_view = findViewById(R.id.shared_memory_value);
            String value_str = shared_memory_value_str_view.getText().toString();
            if (value_str.isEmpty()) {
                this.shared_memory_value = 9000;
            } else {
                this.shared_memory_value = Long.parseLong(shared_memory_value_str_view.getText().toString());
            }
            sharedMemoryWriteValueNative();
            Toast.makeText(getApplicationContext(),
                    "Wrote to ashmem:" + this.shared_memory_value,
                    Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(getApplicationContext(), "Turn on the unix domain socket server",
                    Toast.LENGTH_SHORT).show();
        }
    }

    public void readTimestampInNative(View view) {
        if(isDomainSocketServerStated == true) {
            Toast.makeText(getApplicationContext(),
                    "Read from ashmem:" + ashmemReadTimestampNative(),
                    Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(getApplicationContext(), "Turn on the unix domain socket server",
                    Toast.LENGTH_SHORT).show();
        }
    }

    public void readTimestampInJava(View view) {
        if (sharedMemoryType == 1 && isDomainSocketServerStated == true) {
            if (ashmemBuffer == null) {
                FileInputStream ashmemFIS = new FileInputStream(getAshmemJavaFd());
                FileChannel fc = ashmemFIS.getChannel();
                try {
                    ashmemBuffer = fc.map(FileChannel.MapMode.READ_ONLY, 0, 8);
                    ashmemBuffer.order(ByteOrder.LITTLE_ENDIAN);
                    ashmemBuffer.load();
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
        } else {
            Toast.makeText(getApplicationContext(), "Turn on the unix domain socket server",
                    Toast.LENGTH_SHORT).show();
        }
    }

    public native void ashmemServer();

    public native void sharedMemoryWriteValueNative();

    public native String ashmemReadTimestampNative();

    public native FileDescriptor getAshmemJavaFd();
}

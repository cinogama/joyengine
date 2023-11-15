package net.cinogama.joyengineecs4a;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.content.res.AssetManager;

import com.google.androidgamesdk.GameActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class MainActivity extends GameActivity {
    static {
        System.loadLibrary("joyengineecs4a");
    }

    public native void initJoyEngine(
        String native_lib_path,
        String cache_path,
        String asset_path);

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Check packing files, varify file version. unpack them.
        try {
            File dir = new File(getApplication().getCacheDir(), "builtin");
            if (!dir.exists())
            {
                dir.mkdirs();
            }
            AssetManager asset_manager = getAssets();
            for (String path : asset_manager.list(""))
            {
                if (path.endsWith(".jeimg4") || path.endsWith(".jeimgidx4"))
                {
                    InputStream is = asset_manager.open(path);
                    FileOutputStream fos = new FileOutputStream(
                            new File(getApplication().getFilesDir().getAbsolutePath(), path));
                    byte[] buffer = new byte[1024];
                    int byteCount=0;
                    while((byteCount=is.read(buffer))!=-1) {//循环从输入流读取 buffer字节
                        fos.write(buffer, 0, byteCount);//将读取的输入流写入到输出流
                    }
                    fos.flush();//刷新缓冲区
                    is.close();
                    fos.close();
                }
            }
        }
        catch(Exception e)
        {
            Log.e("JoyEngineAssets", "Failed to unpack image: " + e.getMessage());
        }

        // Init native lib path.
        initJoyEngine(
                getApplicationInfo().nativeLibraryDir,
                getApplication().getCacheDir().getAbsolutePath(),
                getApplication().getFilesDir().getAbsolutePath());
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) {
            hideSystemUi();
        }
    }

    private void hideSystemUi() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN
        );
    }
}
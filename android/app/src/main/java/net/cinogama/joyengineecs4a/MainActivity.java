package net.cinogama.joyengineecs4a;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.content.Context;
import android.view.inputmethod.InputMethodManager;
import android.view.KeyEvent;
import android.content.res.AssetManager;

import java.util.concurrent.LinkedBlockingQueue;

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
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    public void showSoftInput() {
        InputMethodManager inputMethodManager = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        inputMethodManager.showSoftInput(this.getWindow().getDecorView(), 0);
    }
    public void  hideSoftInput() {
        InputMethodManager inputMethodManager = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        inputMethodManager.hideSoftInputFromWindow(this.getWindow().getDecorView().getWindowToken(), 0);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) {
            hideSystemUi();
        }
    }

    // Queue for the Unicode characters to be polled from native code (via pollUnicodeChar())
    private LinkedBlockingQueue<Integer> unicodeCharacterQueue = new LinkedBlockingQueue<Integer>();

    // We assume dispatchKeyEvent() of the NativeActivity is actually called for every
    // KeyEvent and not consumed by any View before it reaches here
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            unicodeCharacterQueue.offer(event.getUnicodeChar(event.getMetaState()));
        }
        return super.dispatchKeyEvent(event);
    }

    int pollUnicodeChar() {
        Integer val = unicodeCharacterQueue.poll();
        if (val == null)
            return 0;
        return val;
    }
}
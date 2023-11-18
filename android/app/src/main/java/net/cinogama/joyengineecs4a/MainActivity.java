package net.cinogama.joyengineecs4a;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.content.Context;
import android.view.inputmethod.InputMethodManager;
import android.view.KeyEvent;

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
        AssetManager asset_manager,
        String native_lib_path,
        String cache_path,
        String asset_path);

    public void doInitJoyEngineBasicConfig()
    {
        // Check packing files, varify file version. unpack them.
        try {
            File dir = new File(getApplication().getCacheDir(), "builtin");
            if (!dir.exists())
            {
                dir.mkdirs();
            }
        }
        catch(Exception e)
        {
            Log.e("JoyEngineAssets", "Assets failed: " + e.getMessage());
        }

        // Init native lib path.
        initJoyEngine(
                getAssets(),
                getApplicationInfo().nativeLibraryDir,
                getApplication().getCacheDir().getAbsolutePath(),
                getApplication().getFilesDir().getAbsolutePath());
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
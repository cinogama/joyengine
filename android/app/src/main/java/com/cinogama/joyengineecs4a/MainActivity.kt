package com.cinogama.joyengineecs4a

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.cinogama.joyengineecs4a.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        // Example of a call to a native method
        binding.sampleText.text = entry(applicationInfo.nativeLibraryDir)
    }

    /**
     * A native method that is implemented by the 'joyengineecs4a' native library,
     * which is packaged with this application.
     */
    external fun entry(external_path: String): String

    companion object {
        // Used to load the 'joyengineecs4a' library on application startup.
        init {
            System.loadLibrary("joyengineecs4a")
            System.loadLibrary("woo_debug")
            // System.loadLibrary("abababa")
        }

    }
}
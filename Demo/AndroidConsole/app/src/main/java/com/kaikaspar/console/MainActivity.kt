package com.kaikaspar.console

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView

class MainActivity : Activity() {
    private lateinit var language: Spinner
    private lateinit var source: EditText
    private lateinit var output: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        language = Spinner(this).apply {
            adapter = ArrayAdapter(
                this@MainActivity,
                android.R.layout.simple_spinner_dropdown_item,
                listOf("Rho", "Pi", "Tau")
            )
        }

        source = EditText(this).apply {
            minLines = 8
            gravity = Gravity.TOP or Gravity.START
            setSingleLine(false)
            setText("1 2 +")
            hint = "Enter Rho, Pi, or Tau source"
        }

        output = TextView(this).apply {
            textSize = 14f
            setTextIsSelectable(true)
        }

        val run = Button(this).apply {
            text = "Run"
            setOnClickListener {
                val selectedLanguage = language.selectedItem.toString()
                val result = KaiConsoleBridge.evaluate(selectedLanguage, source.text.toString())
                appendOutput(selectedLanguage, result)
            }
        }

        val outputScroll = ScrollView(this).apply {
            addView(output)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                0,
                1f
            )
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(24, 24, 24, 24)
            addView(language)
            addView(source)
            addView(run)
            addView(outputScroll)
        }

        setContentView(root)
    }

    private fun appendOutput(selectedLanguage: String, result: String) {
        output.append("> $selectedLanguage\n")
        output.append(result)
        output.append("\n\n")
    }
}

object KaiConsoleBridge {
    init {
        System.loadLibrary("kai_console_bridge")
    }

    external fun evaluate(language: String, source: String): String
}

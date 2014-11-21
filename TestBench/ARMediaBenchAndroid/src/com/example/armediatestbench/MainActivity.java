/*
    Copyright (C) 2014 Parrot SA

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Parrot nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/
package com.example.armediatestbench;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;

import android.app.Activity;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.widget.Button;

import com.parrot.arsdk.armedia.ARMediaManager;
import com.parrot.arsdk.armedia.ARMediaNotificationReceiver;
import com.parrot.arsdk.armedia.ARMediaNotificationReceiverListener;

public class MainActivity extends Activity implements ARMediaNotificationReceiverListener
{

    private static final String TAG = "MainActivity";
    private ARMediaNotificationReceiver notificationReceiver;
    static
    {
        try
        {
            System.loadLibrary("ardiscovery");
            System.loadLibrary("ardiscovery_android");
            System.loadLibrary("ardatatransfer");
            System.loadLibrary("ardatatransfer_android");
            System.loadLibrary("armedia");
            System.loadLibrary("armedia_android");
        }
        catch (Exception e)
        {
            Log.e(TAG, "Oops (LoadLibrary)", e);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        final ARMediaManager mediaManager = ARMediaManager.getInstance(getApplicationContext());
        initBroadcastReceivers();
        registerReceivers();
        
        final Button buttonInit = (Button) findViewById(R.id.button1);
        buttonInit.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
                ArrayList<String> list = new ArrayList<String>();
                list.add("ARDrone");
                list.add("Jumping Sumo");
                list.add("bebop");
                mediaManager.initWithProjectIDs(list);
            }
        });

        final Button buttonUpdate = (Button) findViewById(R.id.button3);
        buttonUpdate.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
                mediaManager.update();
            }
        });

        final Button buttonRetreive = (Button) findViewById(R.id.button4);
        buttonRetreive.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
                HashMap<String, Object> dico = new HashMap<>();
                dico = mediaManager.retrieveProjectsDictionary(null);
                Log.d(TAG, "dico:" + dico);
            }
        });

        final Button buttonAdd = (Button) findViewById(R.id.button2);
        buttonAdd.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
                /*getResources().openRawResource(R.drawable.bebop);
                Bitmap bm = BitmapFactory.decodeResource(getResources(), R.drawable.bebop);
                String extStorageDirectory = Environment.getExternalStorageDirectory().toString().concat("/Video/AR.Drone_4ACE4FDBCC3D0F40524DD3D46407AEE7_1970-01-01T000222+0000.mp4");
                Log.d(TAG, extStorageDirectory);
                File file = new File("res/", "bebop.jpg");
                FileOutputStream outStream = null;
                try
                {
                    outStream = new FileOutputStream(file);
                    bm.compress(Bitmap.CompressFormat.JPEG, 100, outStream);
                    outStream.flush();
                    outStream.close();
                }
                catch (FileNotFoundException e)
                {
                    e.printStackTrace();
                }
                catch (IOException e)
                {
                    e.printStackTrace();
                }
               */
                //File directory = new File(Environment.getExternalStorageDirectory().toString().concat("/Video/AR.Drone_4ACE4FDBCC3D0F40524DD3D46407AEE7_1970-01-01T000222+0000.mp4"));
                
                
                /*try{
                    InputStream inputStream = getResources().openRawResource(R.drawable.bebop);
                    File tempFile = File.createTempFile("pre", ".jpg");
                    copyFile(inputStream, new FileOutputStream(tempFile));
                    Log.d(TAG, "add file :" + tempFile.getAbsolutePath());
                    mediaManager.addMedia(tempFile);
                    // Now some_file is tempFile .. do what you like
                  } catch (IOException e) {
                    throw new RuntimeException("Can't create temp file ", e);
                  }
                */
                File directory = new File(Environment.getExternalStorageDirectory().toString().concat("/Video/s.jpg"));
                if (directory.exists())
                {
                    Log.d(TAG, "add file :" + directory);
                    Log.d(TAG, "add file :" + directory.length());

                    mediaManager.addMedia(directory);
                }
            }
        });
    }
    

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    /**
     * Initialize our local broadcast receivers
     */
    private void initBroadcastReceivers()
    {
        notificationReceiver = new ARMediaNotificationReceiver(this);
    }

    /**
     * Register our local broadcast receivers
     */
    private void registerReceivers()
    {
        // Local receivers
        LocalBroadcastManager lbm = LocalBroadcastManager.getInstance(getApplicationContext());
        lbm.registerReceiver(notificationReceiver, new IntentFilter(ARMediaManager.ARMediaManagerNotificationDictionary));
    }

    /**
     * Unregister our local broadcast receivers
     */
    private void unregisterReceivers()
    {
        // Unregistering local receivers
        LocalBroadcastManager lbm = LocalBroadcastManager.getInstance(getApplicationContext());
        lbm.unregisterReceiver(notificationReceiver);
    }

    @Override
    public void onNotificationDictionaryIsInit()
    {
            Log.d(TAG, "onNotificationDictionaryIsInit" );
    }
    
    @Override
    public void onNotificationDictionaryIsUpdated(Boolean isUpdated)
    {
            Log.d(TAG, "onNotificationDictionaryIsUpdated" + isUpdated);
    }
    
    @Override
    public void onNotificationDictionaryIsUpdating(Double value)
    {
            Log.d(TAG, "onNotificationDictionaryIsUpdating" + value);
    }
    
    @Override
    public void onNotificationDictionaryMediaAdded(String assetUrlPath)
    {
            Log.d(TAG, "onNotificationDictionaryMediaAdded" + assetUrlPath);
    }
    
}
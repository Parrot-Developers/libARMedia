package com.parrot.arsdk.armedia;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class ARMediaNotificationReceiver extends BroadcastReceiver
{
    private ARMediaNotificationReceiverListener listener;
    
    public ARMediaNotificationReceiver(ARMediaNotificationReceiverListener listener)
    {
        this.listener = listener;
    }

    @Override
    public void onReceive(Context context, Intent intent)
    {
        Bundle dictionary = intent.getExtras();
        
        if (listener != null)
        {
            listener.onNotificationDictionaryChanged(dictionary);
        }
    }
}


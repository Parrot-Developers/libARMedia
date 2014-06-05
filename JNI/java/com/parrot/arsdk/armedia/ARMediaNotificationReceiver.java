package com.parrot.arsdk.armedia;

import com.parrot.arsdk.armedia.ARMediaManager;

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
            if (dictionary.containsKey(ARMediaManager.ARMediaManagerNotificationDictionaryIsInitKey))
            {
                listener.onNotificationDictionaryIsInit();
            }
            else if (dictionary.containsKey(ARMediaManager.ARMediaManagerNotificationDictionaryUpdatedKey))
            {
                listener.onNotificationDictionaryIsUpdated((Boolean)dictionary.get(ARMediaManager.ARMediaManagerNotificationDictionaryUpdatedKey));
            }
            else if (dictionary.containsKey(ARMediaManager.ARMediaManagerNotificationDictionaryUpdatingKey))
            {
                listener.onNotificationDictionaryIsUpdating((Double)dictionary.get(ARMediaManager.ARMediaManagerNotificationDictionaryUpdatingKey));
            }
            else if (dictionary.containsKey(ARMediaManager.ARMediaManagerNotificationDictionaryMediaAddedKey))
            {
                listener.onNotificationDictionaryMediaAdded((String)dictionary.get(ARMediaManager.ARMediaManagerNotificationDictionaryMediaAddedKey));
            }
        }
    }
}


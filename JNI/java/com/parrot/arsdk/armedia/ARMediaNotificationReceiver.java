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


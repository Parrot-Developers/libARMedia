package com.parrot.arsdk.armedia;

import android.os.Bundle;

public interface ARMediaNotificationReceiverListener 
{
    public void onNotificationDictionaryIsInit();
    public void onNotificationDictionaryIsUpdated(Boolean isUpdated);
    public void onNotificationDictionaryIsUpdating(Double value);
    public void onNotificationDictionaryMediaAdded(String assetUrlPath);
}


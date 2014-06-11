package com.parrot.arsdk.armedia;

import android.os.Bundle;

public interface ARMediaNotificationReceiverListener 
{
    public void onNotificationDictionaryIsInit();
    public void onNotificationDictionaryIsUpdated(boolean isUpdated);
    public void onNotificationDictionaryIsUpdating(double value);
    public void onNotificationDictionaryMediaAdded(String assetUrlPath);
}


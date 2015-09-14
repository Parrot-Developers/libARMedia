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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.json.JSONException;
import org.json.JSONObject;

import android.R.integer;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.database.MergeCursor;
import android.graphics.drawable.Drawable;
import android.media.Image;
import android.net.Uri;
import android.nfc.tech.IsoDep;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.MediaStore.Images;
import android.provider.MediaStore.Images.Media;
import android.provider.MediaStore.Video;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Base64;
import android.util.Base64InputStream;
import android.util.Base64OutputStream;
import android.util.Log;

import com.parrot.arsdk.armedia.ARMEDIA_ERROR_ENUM;
import com.parrot.arsdk.armedia.ARMediaObject;
import com.parrot.arsdk.arsal.ARSALPrint;
import com.parrot.arsdk.armedia.MEDIA_TYPE_ENUM;
import com.parrot.arsdk.ardiscovery.ARDiscoveryService;
import com.parrot.arsdk.armedia.ARMediaVideoAtoms;
import com.parrot.arsdk.ardiscovery.ARDISCOVERY_PRODUCT_ENUM;

public class ARMediaManager
{
    /* Singleton part */
    static private ARMediaManager instance = null;

    static public ARMediaManager getInstance(Context context)
    {
        if (instance == null)
        {
            instance = new ARMediaManager(context);
        }
        return instance;
    }

    private String TAG = ARMediaManager.class.getSimpleName();

    private Context context;
    private ContentResolver contentResolver;

    private static final String ARMEDIA_MANAGER_DATABASE_FILENAME = "ARMediaDB";

    public static final String ARMEDIA_MANAGER_JPG = ".jpg";
    public static final String ARMEDIA_MANAGER_MP4 = ".mp4";
    public static final String ARMEDIA_MANAGER_MOV = ".mov";

    public static final String DOWNLOADING_PREFIX = "downloading_";
    public static final String LOCAL_MEDIA_MASS_STORAGE_PATH =
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM).getAbsolutePath() + File.separator;

    private static final String kARMediaManagerKey = "kARMediaManagerKey";
    private static final String kARMediaManagerProjectDicCount = "kARMediaManagerProjectDicCount";

    // PVAT Keys
    public static final String ARMediaManagerPVATRunDateKey = "run_date";
    public static final String ARMediaManagerPVATMediaDateKey = "media_date";
    public static final String ARMediaManagerPVATProductIdKey = "product_id";
    public static final String ARMediaManagerPVATuuidKey = "uuid";

    public final static String ARMediaManagerNotificationDictionary = "ARMediaManagerNotificationDictionary";
    public final static String ARMediaManagerNotificationDictionaryIsInitKey = "ARMediaManagerNotificationDictionaryIsInitKey";
    public final static String ARMediaManagerNotificationDictionaryUpdatingKey = "ARMediaManagerNotificationDictionaryUpdatingKey";
    public final static String ARMediaManagerNotificationDictionaryUpdatedKey = "ARMediaManagerNotificationDictionaryUpdatedKey";
    public final static String ARMediaManagerNotificationDictionaryMediaAddedKey = "ARMediaManagerNotificationDictionaryMediaAddedKey";

    private final static String ARMediaManagerProjectProductName = "ARMediaManagerProjectProductName";
    private final static String ARMediaManagerProjectAssetURLKey = "ARMediaManagerProjectAssetURLKey";
    private final static String ARMediaManagerObjectRunDate = "ARMediaManagerObjectRunDate";
    private final static String ARMediaManagerObjectProduct = "ARMediaManagerObjectProduct";
    private final static String ARMediaManagerObjectProductId = "ARMediaManagerObjectProductId";
    private final static String ARMediaManagerObjectName = "ARMediaManagerObjectName";
    private final static String ARMediaManagerObjectDate = "ARMediaManagerObjectDate";
    private final static String ARMediaManagerObjectFilePath = "ARMediaManagerObjectFilePath";
    private final static String ARMediaManagerObjectSize = "ARMediaManagerObjectSize";
    private final static String ARMediaManagerObjectMediaType = "ARMediaManagerObjectMediaType";

    private boolean isInit = false;
    private boolean isUpdate = false;

    private HashMap<String, Object> projectsDictionary;
    private Activity currentActivity;
    private int valueKARMediaManagerKey;

    private ARMediaManager(Context context)
    {
        this.context = context;
        this.contentResolver = context.getContentResolver();
        isInit = false;
        isUpdate = false;
        projectsDictionary = new HashMap<String, Object>();
    }

    public ARMEDIA_ERROR_ENUM initWithProjectIDs(ArrayList<String> projectIDs)
    {
        ARMEDIA_ERROR_ENUM returnVal = ARMEDIA_ERROR_ENUM.ARMEDIA_OK;
        if (!isInit)
        {
            for (String key : projectIDs)
            {
                if (!projectsDictionary.containsKey(key))
                {
                    projectsDictionary.put(key, new HashMap<String, Object>());
                }
            }

            /* dictionary of update */
            Bundle notificationBundle = new Bundle();
            notificationBundle.putString(ARMediaManagerNotificationDictionaryIsInitKey, "");

            /* send NotificationDictionaryChanged */
            Intent intentDicChanged = new Intent(ARMediaManagerNotificationDictionary);
            intentDicChanged.putExtras(notificationBundle);
            LocalBroadcastManager.getInstance(context).sendBroadcast(intentDicChanged);

            isInit = true;
            returnVal = ARMEDIA_ERROR_ENUM.ARMEDIA_OK;

        }

        return returnVal;
    }

    public HashMap<String, Object> retrieveProjectsDictionary(String project)
    {
        HashMap<String, Object> retProjectictionary = new HashMap<String, Object>();

        if (project == null)
        {
            for (String key : projectsDictionary.keySet())
            {
                retProjectictionary.put(key, projectsDictionary.get(key));
            }
        }
        else
        {
            retProjectictionary.put(project, projectsDictionary.get(project));
        }

        return projectsDictionary;
    }

    public ARMEDIA_ERROR_ENUM update()
    {
        ARSALPrint.v(TAG, "update MediaManager");
        if (!isInit)
            return ARMEDIA_ERROR_ENUM.ARMEDIA_ERROR_MANAGER_NOT_INITIALIZED;

        isUpdate = false;
        int totalMediaInFoldersCount = 0;
        for (String key : projectsDictionary.keySet())
        {
            int nbrOfFileInFolder = 0;
            String directoryName = LOCAL_MEDIA_MASS_STORAGE_PATH + key;
            File directory = new File(directoryName);

            if (directory.listFiles() != null)
            {
                nbrOfFileInFolder = directory.listFiles().length;
                totalMediaInFoldersCount += nbrOfFileInFolder;
            }
        }

        // Get all asset
        for (String key : projectsDictionary.keySet())
        {

            String[] requestedColumnsImg = { Images.Media.TITLE, Images.Media.DATA, };
            String[] requestedColumnsVideo = { Video.Media.TITLE, Video.Media.DATA, };

            String selection = MediaStore.Images.Media.BUCKET_DISPLAY_NAME + " =?";
            String[] selectionArgs = new String[] { key.toString() };

            final Cursor cursorVideoExterne = context.getContentResolver().query(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, requestedColumnsVideo, selection, selectionArgs, null);

            final Cursor cursorPhotoExterne = context.getContentResolver().query(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, requestedColumnsImg, selection, selectionArgs, null);

            final Cursor cursorVideoInterne = context.getContentResolver().query(MediaStore.Video.Media.INTERNAL_CONTENT_URI, requestedColumnsVideo, selection, selectionArgs, null);

            final Cursor cursorPhotoInterne = context.getContentResolver().query(MediaStore.Images.Media.INTERNAL_CONTENT_URI, requestedColumnsImg, selection, selectionArgs, null);

            MergeCursor cursorPhoto = new MergeCursor(new Cursor[] { cursorPhotoExterne, cursorPhotoInterne });
            MergeCursor cursorVideo = new MergeCursor(new Cursor[] { cursorVideoExterne, cursorVideoInterne });

            int currentCount = 0;
            arMediaManagerNotificationUpdating((double) currentCount / (double) totalMediaInFoldersCount * 100);
            JSONObject jsonReader;
            String exifProductID = null;

            try
            {
                if (!cursorPhoto.moveToFirst())
                {
                    ARSALPrint.v(TAG, "No Photo files for album: " + key);
                }
                else
                {
                    do
                    {
                        String mediaFileAbsolutPath = cursorPhoto.getString(cursorPhoto.getColumnIndex("_data"));
                        String mediaFilePath = cursorPhoto.getString(cursorPhoto.getColumnIndex("_data")).substring(Environment.getExternalStorageDirectory().toString().length());
                        String mediaName = cursorPhoto.getString(cursorPhoto.getColumnIndex("title"));
                        if (mediaFileAbsolutPath.endsWith(ARMEDIA_MANAGER_JPG))
                        {
                            Exif2Interface exif;
                            try
                            {
                                exif = new Exif2Interface(mediaFileAbsolutPath);
                                String description = exif.getAttribute(Exif2Interface.Tag.IMAGE_DESCRIPTION);
                                ARSALPrint.v(TAG, "image:"+mediaFileAbsolutPath+", desc="+description);
                                if (description != null)
                                {
                                    jsonReader = new JSONObject(description);
                                    if (jsonReader.has(ARMediaManagerPVATProductIdKey))
                                        exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                                    String productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));
                                    ARSALPrint.v(TAG, "image product="+productName);
                                    if (projectsDictionary.keySet().contains(productName))
                                    {
                                        HashMap<String, Object> hashMap = (HashMap<String, Object>) projectsDictionary.get(jsonReader.getString(ARMediaManagerPVATProductIdKey));
                                        if ((hashMap == null) || (!hashMap.containsKey(mediaFilePath)))
                                        {
                                            ARMediaObject mediaObject = createMediaObjectFromJson(mediaFilePath, jsonReader);
                                            if(mediaObject != null)
                                            {
                                                mediaObject.mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_PHOTO;
                                                ARSALPrint.v(TAG, "add photo:"+mediaFilePath);
                                                ((HashMap<String, Object>) projectsDictionary.get(productName)).put(mediaFilePath, mediaObject);
                                            }
                                        }
                                    }
                                }
                            }
                            catch (IOException e)
                            {
                                // TODO Auto-generated catch block
                                e.printStackTrace();
                            }
                            catch (JSONException e)
                            {
                                // TODO: handle exception
                                e.printStackTrace();
                            }
                        }
                        currentCount++;
                        arMediaManagerNotificationUpdating((double) currentCount / (double) totalMediaInFoldersCount * 100);
                    }
                    while (cursorPhoto.moveToNext());
                }
            }
            finally
            {
                if (cursorPhoto != null)
                {
                    cursorPhoto.close();
                }
            }

            try
            {
                if (!cursorVideo.moveToFirst())
                {
                    ARSALPrint.d(TAG, "No Video for :" + key);
                }
                else
                {
                    do
                    {
                        String mediaFileAbsolutPath = cursorVideo.getString(cursorVideo.getColumnIndex("_data"));
                        String mediaName = cursorVideo.getString(cursorVideo.getColumnIndex("title"));
                        if (!mediaFileAbsolutPath.contains(DOWNLOADING_PREFIX) && isValidVideoFile(mediaFileAbsolutPath))
                        {
                            ARSALPrint.v(TAG, "adding video:"+mediaFileAbsolutPath);
                            addARMediaVideoToProjectDictionary(mediaFileAbsolutPath);
                        }
                        currentCount++;
                        arMediaManagerNotificationUpdating((double) currentCount / (double) totalMediaInFoldersCount * 100);
                    }
                    while (cursorVideo.moveToNext());
                }
            }
            finally
            {
                if (cursorVideo != null)
                {
                    cursorVideo.close();
                }
            }

            String directoryName = LOCAL_MEDIA_MASS_STORAGE_PATH + key;
            File directory = new File(directoryName);
            File[] fList = directory.listFiles();
            if (fList != null)
            {
                ARSALPrint.v(TAG, "Fetching files in DCIM folder");
                for (File file : fList)
                {
                    final String filePath = file.getAbsolutePath();
                    if (!filePath.contains(DOWNLOADING_PREFIX) && isValidVideoFile(filePath))
                    {
                        ARSALPrint.v(TAG, "adding video:"+filePath);
                        addARMediaVideoToProjectDictionary(filePath);
                    }
                }
            }
        }

        /* dictionary of update */
        Bundle notificationBundle = new Bundle();
        notificationBundle.putBoolean(ARMediaManagerNotificationDictionaryUpdatedKey, true);

        /* send NotificationDictionaryChanged */
        Intent intentDicChanged = new Intent(ARMediaManagerNotificationDictionary);
        intentDicChanged.putExtras(notificationBundle);
        LocalBroadcastManager.getInstance(context).sendBroadcast(intentDicChanged);
        isUpdate = true;

        return ARMEDIA_ERROR_ENUM.ARMEDIA_OK;
    }

    public boolean addMedia(File mediaFile)
    {
        boolean returnVal = false;

        if (!isUpdate)
            return returnVal;

        isUpdate = false;
        returnVal = saveMedia(mediaFile);

        return returnVal;
    }

    public boolean isUpdate()
    {
        return isUpdate;
    }

    /***
     * 
     * Private Methods
     * 
     */

    private boolean saveMedia(File file)
    {
        boolean added = false;
        boolean toAdd = false;
        int productID;
        ARMediaObject mediaObject = null;
        String productName = null;
        String exifProductID = null;
        JSONObject jsonReader;
        String filename = file.getName();

        ARSALPrint.v(TAG, "Save media:"+file.getPath());
        if (filename.endsWith(ARMEDIA_MANAGER_JPG))
        {
            Exif2Interface exif;
            try
            {
                exif = new Exif2Interface(file.getPath());
                String description = exif.getAttribute(Exif2Interface.Tag.IMAGE_DESCRIPTION);
                ARSALPrint.v(TAG, "image description:"+description);
                if (description != null)
                {
                    jsonReader = new JSONObject(description);
                    if (jsonReader.has(ARMediaManagerPVATProductIdKey))
                        exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));
                    ARSALPrint.v(TAG, "new image product="+productName);
                    if (projectsDictionary.keySet().contains(productName))
                    {
                        productID = Integer.parseInt(jsonReader.getString(ARMediaManagerPVATProductIdKey), 16);
                        mediaObject = createMediaObjectFromJson(
                                File.separator + Environment.DIRECTORY_DCIM + File.separator + productName + File.separator  + filename, jsonReader);
                        if(mediaObject != null)
                        {
                            ARSALPrint.v(TAG, "new image path:"+mediaObject.getFilePath());
                            mediaObject.mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_PHOTO;
                        }
                        toAdd = true;
                    }
                }
            }
            catch (IOException e)
            {
                e.printStackTrace();
            }
            catch (Exception e)
            {
                e.printStackTrace();
            }

        }
        else if (isValidVideoFile(filename))
        {
            String description = com.parrot.arsdk.armedia.ARMediaVideoAtoms.getPvat(file.getAbsolutePath());
            ARSALPrint.v(TAG, "video description:"+description);
            if (description != null)
            {
                try
                {
                    jsonReader = new JSONObject(description);
                    if (jsonReader.has(ARMediaManagerPVATProductIdKey))
                        exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));
                    ARSALPrint.v(TAG, "new video product="+productName);
                    if (projectsDictionary.keySet().contains(productName))
                    {
                        productID = Integer.parseInt(jsonReader.getString(ARMediaManagerPVATProductIdKey), 16);
                        mediaObject = createMediaObjectFromJson(
                                File.separator + Environment.DIRECTORY_DCIM + File.separator + productName + File.separator  + filename, jsonReader);
                        if(mediaObject != null)
                        {
                            ARSALPrint.v(TAG, "new video path:"+mediaObject.getFilePath());
                            mediaObject.mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_VIDEO;
                        }
                        toAdd = true;
                    }
                }
                catch (JSONException e)
                {
                    e.printStackTrace();
                }
                catch (Exception e)
                {
                    e.printStackTrace();
                }
            }
        }
        else
        {
            added = false;
        }

        if (toAdd)
        {
            String directory = LOCAL_MEDIA_MASS_STORAGE_PATH + productName;
            File directoryFolder = new File(directory);
            if (!directoryFolder.exists() || !directoryFolder.isDirectory()) 
            {
                directoryFolder.mkdir();
            }
            File destination = new File(directory + File.separator + filename);
            if (mediaObject != null)
            {
                context.sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, Uri.fromFile(destination)));
                ((HashMap<String, Object>) projectsDictionary.get(productName)).put(destination.getPath().substring(Environment.getExternalStorageDirectory().toString().length()), mediaObject);
                arMediaManagerNotificationMediaAdded(destination.getPath().substring(Environment.getExternalStorageDirectory().toString().length()));
                added = true;
            }
        }

        isUpdate = true;
        return added;
    }

    private void addARMediaVideoToProjectDictionary(String mediaFileAbsolutPath)
    {
        String mediaFilePath = mediaFileAbsolutPath.substring(Environment.getExternalStorageDirectory().toString().length());
        String description = com.parrot.arsdk.armedia.ARMediaVideoAtoms.getPvat(mediaFileAbsolutPath);
        if (description != null)
        {
            String atomProductID = null;
            try
            {
                JSONObject jsonReader = new JSONObject(description);
                if (jsonReader.has(ARMediaManagerPVATProductIdKey))
                    atomProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                String productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(atomProductID, 16)));
                if (projectsDictionary.keySet().contains(productName))
                {
                    HashMap<String, Object> hashMap = (HashMap<String, Object>) projectsDictionary.get(jsonReader.getString(ARMediaManagerPVATProductIdKey));
                    if ((hashMap == null) || (!hashMap.containsKey(mediaFilePath)))
                    {
                        ARMediaObject mediaObject = createMediaObjectFromJson(mediaFilePath, jsonReader);
                        if (mediaObject != null)
                        {
                            mediaObject.mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_VIDEO;
                            ((HashMap<String, Object>) projectsDictionary.get(productName)).put(mediaFilePath, mediaObject);
                        } 
                    }
                }
            }
            catch (JSONException e)
            {
                e.printStackTrace();
            }
        }
    }

    private ARMediaObject createMediaObjectFromJson(String mediaPath, JSONObject jsonReader)
    {
        ARMediaObject mediaObject = null;

        if(jsonReader != null)
        {
            mediaObject = new ARMediaObject();
            try
            {
                mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                mediaObject.product = ARDiscoveryService.getProductFromProductID((int) Long.parseLong(mediaObject.productId, 16));
                if (jsonReader.has(ARMediaManagerPVATMediaDateKey))
                    mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                if (jsonReader.has(ARMediaManagerPVATRunDateKey))
                    mediaObject.runDate = jsonReader.getString(ARMediaManagerPVATRunDateKey);
                if (jsonReader.has(ARMediaManagerPVATuuidKey))
                    mediaObject.uuid = jsonReader.getString(ARMediaManagerPVATuuidKey);
                mediaObject.filePath = mediaPath;
            }
            catch (JSONException e)
            {
                e.printStackTrace();
            }
        }
        return mediaObject;
    }

    private void arMediaManagerNotificationMediaAdded(String mediaPath)
    {
        /* dictionary of update */
        Bundle notificationBundle = new Bundle();
        notificationBundle.putString(ARMediaManagerNotificationDictionaryMediaAddedKey, mediaPath);

        /* send NotificationDictionaryChanged */
        Intent intentDicChanged = new Intent(ARMediaManagerNotificationDictionary);
        intentDicChanged.putExtras(notificationBundle);
        LocalBroadcastManager.getInstance(context).sendBroadcast(intentDicChanged);
    }

    private void arMediaManagerNotificationUpdating(double percent)
    {
        /* dictionary of update */
        Bundle notificationBundle = new Bundle();
        notificationBundle.putDouble(ARMediaManagerNotificationDictionaryUpdatingKey, percent);

        /* send NotificationDictionaryChanged */
        Intent intentDicChanged = new Intent(ARMediaManagerNotificationDictionary);
        intentDicChanged.putExtras(notificationBundle);
        LocalBroadcastManager.getInstance(context).sendBroadcast(intentDicChanged);
    }

    private boolean isValidVideoFile(String filename)
    {
        return (filename.endsWith(ARMEDIA_MANAGER_MP4) || filename.endsWith(ARMEDIA_MANAGER_MOV));
    }
}

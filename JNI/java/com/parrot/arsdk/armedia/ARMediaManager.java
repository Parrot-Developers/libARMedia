package com.parrot.arsdk.armedia;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
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

    private static final String ARMEDIA_MANAGER_JPG = ".jpg";
    private static final String ARMEDIA_MANAGER_MP4 = ".mp4";

    private static final String kARMediaManagerKey = "kARMediaManagerKey";

    // PVAT Keys
    private static final String ARMediaManagerPVATRunDateKey = "run_date";
    private static final String ARMediaManagerPVATMediaDateKey = "media_date";
    private static final String ARMediaManagerPVATProductIdKey = "product_id";

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
            // Get the productList from the saved file
            SharedPreferences settings = context.getSharedPreferences(ARMEDIA_MANAGER_DATABASE_FILENAME, Context.MODE_PRIVATE);

            this.valueKARMediaManagerKey = settings.getInt(kARMediaManagerKey, -1);

            if (this.valueKARMediaManagerKey > 0)
            {

                byte[] bytes = settings.getString(ARMEDIA_MANAGER_DATABASE_FILENAME, "{}").getBytes();
                if (bytes.length == 0)
                {
                    return null;
                }
                try
                {
                    ByteArrayInputStream byteArray = new ByteArrayInputStream(bytes);
                    Base64InputStream base64InputStream = new Base64InputStream(byteArray, Base64.DEFAULT);
                    ObjectInputStream in;
                    in = new ObjectInputStream(base64InputStream);
                    projectsDictionary = (HashMap<String, Object>) in.readObject();
                }
                catch (IOException e)
                {
                    e.printStackTrace();
                }
                catch (ClassNotFoundException e)
                {
                    e.printStackTrace();
                }
            }

            for (String key : projectIDs)
            {
                if (projectsDictionary.get(key) == null)
                {
                    projectsDictionary.put(key, new HashMap<String, Object>());
                }
            }

            /* dictionary of update */
            Bundle notificationBundle = new Bundle();
            notificationBundle.putString(ARMediaManagerNotificationDictionaryIsInitKey, "isInit");

            /* send NotificationDictionaryChanged */
            Intent intentDicChanged = new Intent(ARMediaManagerNotificationDictionary);
            intentDicChanged.putExtras(notificationBundle);
            LocalBroadcastManager.getInstance(context).sendBroadcast(intentDicChanged);

            isInit = true;
            returnVal = ARMEDIA_ERROR_ENUM.ARMEDIA_OK;

        }

        return returnVal;
    }

    public HashMap<String, Object> retreiveProjectsDictionary(String project)
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

        if (!isInit)
            return ARMEDIA_ERROR_ENUM.ARMEDIA_ERROR_MANAGER_NOT_INITIALIZED;
        isUpdate = false;
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
            int totalMediaCount = cursorPhoto.getCount() + cursorVideo.getCount();

            JSONObject jsonReader;

            try
            {
                if (!cursorPhoto.moveToFirst())
                {
                    Log.d(TAG, "No Photo files for album: " + key);
                }
                else
                {

                    do
                    {
                        
                        String mediaFileAbsolutPath = cursorPhoto.getString(cursorPhoto.getColumnIndex("_data"));
                        String mediaFilePath = cursorPhoto.getString(cursorPhoto.getColumnIndex("_data")).substring(Environment.getExternalStorageDirectory().toString().length());
                        String mediaName = cursorPhoto.getString(cursorPhoto.getColumnIndex("title"));
                        Log.d(TAG, "mediaFilePath :" + mediaFilePath);

                        if (mediaFileAbsolutPath.endsWith(ARMEDIA_MANAGER_JPG))
                        {
                            Exif2Interface exif;
                            try
                            {
                                exif = new Exif2Interface(mediaFileAbsolutPath);
                                String description = exif.getAttribute(Exif2Interface.Tag.IMAGE_DESCRIPTION);

                                // JSONObject jsonReader = new
                                // JSONObject(description);
                                jsonReader = new JSONObject();

                                // Create Json Object using FAKE Data
                                jsonReader.put(ARMediaManagerPVATProductIdKey, "0902");
                                jsonReader.put(ARMediaManagerPVATMediaDateKey, "2013-07-25T160101+0100");
                                jsonReader.put(ARMediaManagerPVATRunDateKey, "2013-07-25T160101+0100");

                                String exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                                String productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));

                                if (projectsDictionary.keySet().contains(productName))
                                {
                                    HashMap<String, Object> hashMap = (HashMap<String, Object>) projectsDictionary.get(jsonReader.getString(ARMediaManagerPVATProductIdKey));

                                    if ((hashMap == null) || (!hashMap.containsKey(mediaFilePath)))
                                    {
                                        ARMediaObject mediaObject = new ARMediaObject();
                                        mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                                        mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                                        mediaObject.runDate = jsonReader.getString(ARMediaManagerPVATRunDateKey);
                                        ((HashMap<String, Object>) projectsDictionary.get(productName)).put(mediaFilePath, mediaObject);
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
                    Log.d(TAG, "No Video for :" + key);
                }
                else
                {

                    do
                    {
                        String mediaFileAbsolutPath = cursorVideo.getString(cursorVideo.getColumnIndex("_data"));
                        String mediaName = cursorVideo.getString(cursorVideo.getColumnIndex("title"));
                        Log.d(TAG, "mediaFileAbsolutPath :" + mediaFileAbsolutPath);

                        if (mediaFileAbsolutPath.endsWith(ARMEDIA_MANAGER_MP4))
                        {
                            addARMediaVideoToProjectDictionary(mediaFileAbsolutPath);
                        }
                        currentCount++;
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

            String directoryName = Environment.getExternalStorageDirectory().toString().concat("/DCIM/").concat(key);

            File directory = new File(directoryName);
            File[] fList = directory.listFiles();

            if (fList != null)
            {
                for (File file : fList)
                {
                    if (file.getAbsolutePath().endsWith(ARMEDIA_MANAGER_MP4))
                    {
                        addARMediaVideoToProjectDictionary(file.getAbsolutePath());
                    }

                }
            }

        }

        int totalMedia = 0;
        for (String key : projectsDictionary.keySet())
        {
            totalMedia += ((HashMap<String, Object>) projectsDictionary.get(key)).size();
        }

        saveMediaOnArchive();

        Log.d(TAG, "projectsDictionary:" + projectsDictionary);

        SharedPreferences settings = context.getSharedPreferences(ARMEDIA_MANAGER_DATABASE_FILENAME, context.MODE_PRIVATE);
        SharedPreferences.Editor editor = settings.edit();
        editor.putInt(kARMediaManagerKey, totalMedia);
        editor.commit();

        isUpdate = true;
        /* dictionary of update */
        Bundle notificationBundle = new Bundle();
        notificationBundle.putString(ARMediaManagerNotificationDictionaryUpdatedKey, "isUpdated");
        
        /* send NotificationDictionaryChanged */
        Intent intentDicChanged = new Intent(ARMediaManagerNotificationDictionary);
        intentDicChanged.putExtras(notificationBundle);
        LocalBroadcastManager.getInstance(context).sendBroadcast(intentDicChanged);

        return ARMEDIA_ERROR_ENUM.ARMEDIA_OK;
    }

    private void addARMediaVideoToProjectDictionary(String mediaFileAbsolutPath)
    {
        String mediaFilePath = mediaFileAbsolutPath.substring(Environment.getExternalStorageDirectory().toString().length());
        String description = com.parrot.arsdk.armedia.ARMediaVideoAtoms.getPvat(mediaFileAbsolutPath);
        try
        {
            // JSONObject jsonReader = new
            // JSONObject(description);

            JSONObject jsonReader = new JSONObject();

            // Create Json Object using FAKE Data
            jsonReader.put(ARMediaManagerPVATProductIdKey, "0902");
            jsonReader.put(ARMediaManagerPVATMediaDateKey, "2013-07-25T160101+0100");
            jsonReader.put(ARMediaManagerPVATRunDateKey, "2013-07-25T160101+0100");

            String atomProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
            String productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(atomProductID, 16)));

            if (projectsDictionary.keySet().contains(productName))
            {
                HashMap<String, Object> hashMap = (HashMap<String, Object>) projectsDictionary.get(jsonReader.getString(ARMediaManagerPVATProductIdKey));
                if ((hashMap == null) || (!hashMap.containsKey(mediaFilePath)))
                {
                    ARMediaObject mediaObject = new ARMediaObject();
                    mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                    mediaObject.runDate = jsonReader.getString(ARMediaManagerPVATRunDateKey);
                    ((HashMap<String, Object>) projectsDictionary.get(productName)).put(mediaFilePath, mediaObject);
                }
            }
        }
        catch (JSONException e)
        {
            e.printStackTrace();
        }
    }

    public boolean addMedia(File mediaFile)
    {
        boolean returnVal = false;

        if (!isUpdate)
            return returnVal;

        isUpdate = false;
        returnVal = saveMedia(mediaFile);

        if (returnVal)
            saveMediaOnArchive();

        return returnVal;
    }

    private boolean saveMedia(File file)
    {
        boolean added = false;
        int productID;
        ARMediaObject mediaObject = new ARMediaObject();
        JSONObject jsonReader;

        String filename = file.getName();

        if (filename.endsWith(ARMEDIA_MANAGER_JPG))
        {
            Exif2Interface exif;
            try
            {
                exif = new Exif2Interface(file.getPath());
                String description = exif.getAttribute(Exif2Interface.Tag.IMAGE_DESCRIPTION);

                // jsonReader = new JSONObject(description);
                jsonReader = new JSONObject();

                // Create Json Object using FAKE Data
                jsonReader.put(ARMediaManagerPVATProductIdKey, "0902");
                jsonReader.put(ARMediaManagerPVATMediaDateKey, "2013-07-25T160101+0100");
                jsonReader.put(ARMediaManagerPVATRunDateKey, "2013-07-25T160101+0100");

                String exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                String productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));

                if (projectsDictionary.keySet().contains(productName))
                {
                    productID = Integer.parseInt(jsonReader.getString(ARMediaManagerPVATProductIdKey), 16);
                    mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                    mediaObject.runDate = jsonReader.getString(ARMediaManagerPVATRunDateKey);

                    ContentValues values = new ContentValues();
                    values.put(Images.Media.TITLE, filename);
                    values.put(Images.Media.DISPLAY_NAME, filename);
                    values.put(Images.Media.BUCKET_DISPLAY_NAME, productName);
                    values.put(Images.Media.DATA, file.getAbsolutePath());
                    values.put(Images.Media.MIME_TYPE, "image/jpg");
                    values.put(Images.Media.DATE_ADDED, System.currentTimeMillis() / 1000);
                    Uri uri = contentResolver.insert(Images.Media.EXTERNAL_CONTENT_URI, values);
                    if (uri != null)
                    {
                        ((HashMap<String, Object>) projectsDictionary.get(productName)).put(uri.getPath(), mediaObject);
                        arMediaManagerNotificationMediaAdded(uri.getPath());
                        added = true;
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
        else if (filename.endsWith(ARMEDIA_MANAGER_MP4))
        {
            String description = com.parrot.arsdk.armedia.ARMediaVideoAtoms.getPvat(file.getAbsolutePath());
            try
            {
                // jsonReader = new JSONObject(description);
                jsonReader = new JSONObject();

                /**/// Create Json Object using FAKE Data
                jsonReader.put(ARMediaManagerPVATProductIdKey, "0902");
                jsonReader.put(ARMediaManagerPVATMediaDateKey, "2013-07-25T160101+0100");
                jsonReader.put(ARMediaManagerPVATRunDateKey, "2013-07-25T160101+0100");
                /**/
                String exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                String productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));

                if (projectsDictionary.keySet().contains(productName))
                {
                    productID = Integer.parseInt(jsonReader.getString(ARMediaManagerPVATProductIdKey), 16);
                    mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                    mediaObject.runDate = jsonReader.getString(ARMediaManagerPVATRunDateKey);

                    ContentValues values = new ContentValues();
                    values.put(Video.Media.TITLE, filename);
                    values.put(Video.Media.DISPLAY_NAME, filename);
                    values.put(Video.Media.BUCKET_DISPLAY_NAME, productName);
                    values.put(Video.Media.DATA, file.getAbsolutePath());
                    values.put(Video.Media.MIME_TYPE, "video/mp4");
                    values.put(Video.Media.DATE_ADDED, System.currentTimeMillis() / 1000);

                    Uri uri = contentResolver.insert(Video.Media.EXTERNAL_CONTENT_URI, values);

                    if (uri != null)
                    {
                        ((HashMap<String, Object>) projectsDictionary.get(productName)).put(uri.getPath(), mediaObject);
                        arMediaManagerNotificationMediaAdded(uri.getPath());
                        added = true;
                    }

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
        else
        {
            added = false;
        }
        isUpdate = true;
        return added;
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

    private void arMediaManagerNotificationUpdating(int percent)
    {
        /* dictionary of update */
        Bundle notificationBundle = new Bundle();
        notificationBundle.putInt(ARMediaManagerNotificationDictionaryUpdatingKey, percent);

        /* send NotificationDictionaryChanged */
        Intent intentDicChanged = new Intent(ARMediaManagerNotificationDictionary);
        intentDicChanged.putExtras(notificationBundle);
    }

    private void saveMediaOnArchive()
    {
        SharedPreferences settings = context.getSharedPreferences(ARMEDIA_MANAGER_DATABASE_FILENAME, context.MODE_PRIVATE);
        SharedPreferences.Editor editor = settings.edit();

        ByteArrayOutputStream arrayOutputStream = new ByteArrayOutputStream();
        try
        {
            ObjectOutputStream objectOutput;
            objectOutput = new ObjectOutputStream(arrayOutputStream);
            objectOutput.writeObject(projectsDictionary);
            byte[] data = arrayOutputStream.toByteArray();
            objectOutput.close();
            arrayOutputStream.close();

            ByteArrayOutputStream out = new ByteArrayOutputStream();
            Base64OutputStream b64 = new Base64OutputStream(out, Base64.DEFAULT);
            b64.write(data);
            b64.close();
            out.close();

            editor.putString(ARMEDIA_MANAGER_DATABASE_FILENAME, new String(out.toByteArray()));

            editor.commit();
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }

    }

    public boolean isUpdate()
    {
        return isUpdate;
    }

}
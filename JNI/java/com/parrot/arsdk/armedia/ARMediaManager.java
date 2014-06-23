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
    private static final String kARMediaManagerProjectDicCount = "kARMediaManagerProjectDicCount";

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

        if (!isInit)
            return ARMEDIA_ERROR_ENUM.ARMEDIA_ERROR_MANAGER_NOT_INITIALIZED;

        isUpdate = false;
        int totalMediaInFoldersCount = 0;
        for (String key : projectsDictionary.keySet())
        {
            int nbrOfFileInFolder = 0;
            String directoryName = Environment.getExternalStorageDirectory().toString().concat("/DCIM/").concat(key);
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
                    Log.d(TAG, "No Photo files for album: " + key);
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
                                jsonReader = new JSONObject(description);
                                if (jsonReader.has(ARMediaManagerPVATProductIdKey))
                                    exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                                String productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));
                                if (projectsDictionary.keySet().contains(productName))
                                {
                                    HashMap<String, Object> hashMap = (HashMap<String, Object>) projectsDictionary.get(jsonReader.getString(ARMediaManagerPVATProductIdKey));
                                    if ((hashMap == null) || (!hashMap.containsKey(mediaFilePath)))
                                    {
                                        ARMediaObject mediaObject = new ARMediaObject();
                                        mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                                        if (jsonReader.has(ARMediaManagerPVATMediaDateKey))
                                            mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                                        if (jsonReader.has(ARMediaManagerPVATRunDateKey))
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
                    Log.d(TAG, "No Video for :" + key);
                }
                else
                {
                    do
                    {
                        String mediaFileAbsolutPath = cursorVideo.getString(cursorVideo.getColumnIndex("_data"));
                        String mediaName = cursorVideo.getString(cursorVideo.getColumnIndex("title"));
                        if (mediaFileAbsolutPath.endsWith(ARMEDIA_MANAGER_MP4))
                        {
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
        ARMediaObject mediaObject = new ARMediaObject();
        String productName = null;
        String exifProductID = null;
        JSONObject jsonReader;
        String filename = file.getName();

        if (filename.endsWith(ARMEDIA_MANAGER_JPG))
        {
            Exif2Interface exif;
            try
            {
                exif = new Exif2Interface(file.getPath());
                String description = exif.getAttribute(Exif2Interface.Tag.IMAGE_DESCRIPTION);
                jsonReader = new JSONObject(description);
                if (jsonReader.has(ARMediaManagerPVATProductIdKey))
                    exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));
                if (projectsDictionary.keySet().contains(productName))
                {
                    productID = Integer.parseInt(jsonReader.getString(ARMediaManagerPVATProductIdKey), 16);
                    mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    if (jsonReader.has(ARMediaManagerPVATMediaDateKey))
                        mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                    if (jsonReader.has(ARMediaManagerPVATRunDateKey))
                        mediaObject.runDate = jsonReader.getString(ARMediaManagerPVATRunDateKey);
                    toAdd = true;
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
                jsonReader = new JSONObject(description);
                if (jsonReader.has(ARMediaManagerPVATProductIdKey))
                    exifProductID = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                productName = ARDiscoveryService.getProductName(ARDiscoveryService.getProductFromProductID((int) Long.parseLong(exifProductID, 16)));
                if (projectsDictionary.keySet().contains(productName))
                {
                    productID = Integer.parseInt(jsonReader.getString(ARMediaManagerPVATProductIdKey), 16);
                    mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    if (jsonReader.has(ARMediaManagerPVATMediaDateKey))
                        mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                    if (jsonReader.has(ARMediaManagerPVATRunDateKey))
                        mediaObject.runDate = jsonReader.getString(ARMediaManagerPVATRunDateKey);
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
        else
        {
            added = false;
        }

        if (toAdd)
        {
            String directory = Environment.getExternalStorageDirectory().toString().concat("/DCIM/").concat(productName);
            File directoryFolder = new File(directory);
            if (!directoryFolder.exists() || !directoryFolder.isDirectory()) 
            {
                directoryFolder.mkdir();
            }
            File destination = new File(directory.concat("/").concat(filename));
            if (copy(file, destination))
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

    private boolean copy(File src, File dst) 
    {
        boolean added = false;
        try 
        {
            InputStream in = new FileInputStream(src);
            OutputStream out = new FileOutputStream(dst);
            byte[] buf = new byte[1024];
            int len;
            while ((len = in.read(buf)) > 0)
            {
                out.write(buf, 0, len);
            }
            out.flush();
            in.close();
            out.close();
            added = true;
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }
        return added;
    }

    private void addARMediaVideoToProjectDictionary(String mediaFileAbsolutPath)
    {
        String mediaFilePath = mediaFileAbsolutPath.substring(Environment.getExternalStorageDirectory().toString().length());
        String description = com.parrot.arsdk.armedia.ARMediaVideoAtoms.getPvat(mediaFileAbsolutPath);
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
                    ARMediaObject mediaObject = new ARMediaObject();
                    mediaObject.productId = jsonReader.getString(ARMediaManagerPVATProductIdKey);
                    if (jsonReader.has(ARMediaManagerPVATMediaDateKey))
                        mediaObject.date = jsonReader.getString(ARMediaManagerPVATMediaDateKey);
                    if (jsonReader.has(ARMediaManagerPVATRunDateKey))
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
}
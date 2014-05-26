
package com.parrot.arsdk.armedia;

import java.io.InputStream;
import java.io.IOException;

import android.util.Log;
import android.graphics.drawable.Drawable;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.content.res.AssetManager;
import android.content.res.Resources;

import com.parrot.arsdk.aracademy.ARAcademyCountry;
import com.parrot.arsdk.aracademy.ARAcademyProfile;
import com.parrot.arsdk.aracademy.ARAcademyUser;
import com.parrot.arsdk.ardatatransfer.ARDataTransferMedia;
import com.parrot.arsdk.ardiscovery.ARDiscoveryService;
import com.parrot.arsdk.ardiscovery.ARDISCOVERY_PRODUCT_ENUM;
import com.parrot.arsdk.armedia.MEDIA_TYPE_ENUM;

import android.graphics.Bitmap;

public class ARMediaObject implements Cloneable
{
    private static final String TAG = ARMediaObject.class.getSimpleName();
    
    private String runDate;
    private ARDISCOVERY_PRODUCT_ENUM product;
    private String productId;
    private String name;
    private String date;
    private String filePath;
    private float size;
    private Drawable thumbnail;
    private MEDIA_TYPE_ENUM mediaType;
    
    public ARMediaObject ()
    {
        name = null;
        filePath = null;
        date = null;
        size = 0.f;
        product = null;
        productId = null;
        thumbnail = null;
        runDate = null;
        mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_MAX;
    }
    
    public void updateDataTransferMedia (Resources resources, ARDataTransferMedia media)
    {
        if (media != null)
        {
            name = media.getName();
            filePath = media.getFilePath();
            date = media.getDate();
            size = media.getSize();
            product = media.getProduct();
            productId = String.format("%04x", ARDiscoveryService.getProductID(product));
            
            Bitmap thumbnailBmp = BitmapFactory.decodeByteArray(media.getThumbnail(), 0, media.getThumbnail().length);
            if(thumbnailBmp != null)
            {
                thumbnail = (Drawable) new BitmapDrawable(resources, thumbnailBmp);
            }
        }
        
        if (name != null)
        {
            String extension = name.substring((name.lastIndexOf(".") + 1), name.length());
            
            if(extension.equals("jpg" /*ARMedia.ARMEDIA_JPG_EXTENSION*/)) //TODO see
            {
                mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_PHOTO;
            }
            else if(extension.equals("mp4" /*ARMedia.ARMEDIA_MP4_EXTENSION*/)) //TODO see
            {
                mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_VIDEO;
            }
        }
    }
    
    public void updateThumbnailWithDataTransferMedia (Resources resources, ARDataTransferMedia media)
    {
        Bitmap thumbnailBmp = BitmapFactory.decodeByteArray(media.getThumbnail(), 0, media.getThumbnail().length);
        
        if(thumbnailBmp != null)
        {
            thumbnail = (Drawable) new BitmapDrawable(resources, thumbnailBmp);
        }
    }
    
    public void updateThumbnailWithUrl (AssetManager assetManager, String assetUrl)
    {
        //TODO see run in thread
        try
        {
            InputStream imgInput = assetManager.open(assetUrl);
            thumbnail = Drawable.createFromStream(imgInput, null);
        }
        catch(final IOException e)
        {
            e.printStackTrace();
        }
    }
    
    public String getRunDate ()
    {
        return runDate;
    }
    
    public ARDISCOVERY_PRODUCT_ENUM getProduct ()
    {
        return product;
    }
    
    public String getProductId ()
    {
        return productId;
    }
    
    public String getName ()
    {
        return name;
    }
    
    public String getDate ()
    {
        return date;
    }
    
    public String getFilePath ()
    {
        return filePath;
    }
    
    public float getSize ()
    {
        return size;
    }
    
    public Drawable getThumbnail ()
    {
        return thumbnail;
    }
    
    public MEDIA_TYPE_ENUM getMediaType ()
    {
        return mediaType;
    }
    
    public Object clone() 
    {
        ARMediaObject model = null;
        try
        {
            /* get instance with super.clone() */
            model = (ARMediaObject) super.clone();
            
            model.runDate = runDate;
            model.product = product;
            model.productId = productId;
            model.name = name;
            model.date = date;
            model.filePath = filePath;
            model.size = size;
            model.thumbnail = thumbnail;
            model.mediaType = mediaType;
        }
        catch(CloneNotSupportedException e)
        {
            e.printStackTrace();
        }
        
        return model;
    }
}

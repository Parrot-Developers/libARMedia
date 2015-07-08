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

import java.io.InputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Comparator;
import java.util.Date;

import android.os.Parcel;
import android.os.Parcelable;
import android.util.Log;
import android.graphics.drawable.Drawable;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.content.res.AssetManager;
import android.content.res.Resources;

import com.parrot.arsdk.ardatatransfer.ARDataTransferMedia;
import com.parrot.arsdk.ardiscovery.ARDiscoveryService;
import com.parrot.arsdk.ardiscovery.ARDISCOVERY_PRODUCT_ENUM;
import com.parrot.arsdk.armedia.MEDIA_TYPE_ENUM;

import android.graphics.Bitmap;

public class ARMediaObject implements Cloneable, Parcelable, Serializable, Comparable<ARMediaObject>
{
    private static final String TAG = ARMediaObject.class.getSimpleName();
    private SimpleDateFormat dateFormater;

    public String runDate;
    public ARDISCOVERY_PRODUCT_ENUM product;
    public String productId;
    public String name;
    public String date;
    public String filePath;
    public float size;
    public Drawable thumbnail;
    public MEDIA_TYPE_ENUM mediaType;
    public String uuid;
    public ARDataTransferMedia media;

    public ARMediaObject()
    {
        name = null;
        filePath = null;
        date = null;
        size = 0.f;
        product = ARDISCOVERY_PRODUCT_ENUM.ARDISCOVERY_PRODUCT_MAX;
        productId = null;
        thumbnail = null;
        runDate = null;
        mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_MAX;
        media = null;
    }

    public void updateDataTransferMedia(Resources resources, ARDataTransferMedia media)
    {
        if (media != null)
        {
            this.media = media;
            name = media.getName();
            filePath = media.getFilePath();
            date = media.getDate();
            size = media.getSize();
            uuid = media.getUUID();

            product = media.getProduct();
            productId = String.format("%04x", ARDiscoveryService.getProductID(product));

            Bitmap thumbnailBmp = BitmapFactory.decodeByteArray(media.getThumbnail(), 0, media.getThumbnail().length);

            if (thumbnailBmp != null)
            {
                thumbnailBmp = Exif2Interface.handleOrientation(thumbnailBmp, media.getThumbnail());
                thumbnail = (Drawable) new BitmapDrawable(resources, thumbnailBmp);
            }
        }

        if (name != null)
        {
            String extension = name.substring((name.lastIndexOf(".") + 1), name.length());

            if (extension.equals("jpg" /*ARMedia.ARMEDIA_JPG_EXTENSION*/)) //TODO see
            {
                mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_PHOTO;
            }
            else if (extension.equals("mp4" /*ARMedia.ARMEDIA_MP4_EXTENSION*/)) //TODO see
            {
                mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_VIDEO;
            }
            else if (extension.equals("mov" /*ARMedia.ARMEDIA_MOV_EXTENSION*/)) //TODO see
            {
                mediaType = MEDIA_TYPE_ENUM.MEDIA_TYPE_VIDEO;
            }
        }
    }

    public void updateThumbnailWithDataTransferMedia(Resources resources, ARDataTransferMedia media)
    {
        this.media = media;
        Bitmap thumbnailBmp = BitmapFactory.decodeByteArray(media.getThumbnail(), 0, media.getThumbnail().length);

        if (thumbnailBmp != null)
        {
            thumbnailBmp = Exif2Interface.handleOrientation(thumbnailBmp, media.getThumbnail());
            thumbnail = (Drawable) new BitmapDrawable(resources, thumbnailBmp);
        }
    }

    public void updateThumbnailWithUrl(AssetManager assetManager, String assetUrl)
    {
        //TODO see run in thread
        try
        {
            InputStream imgInput = assetManager.open(assetUrl);
            thumbnail = Drawable.createFromStream(imgInput, null);
        }
        catch (final IOException e)
        {
            e.printStackTrace();
        }
    }

    public String getRunDate()
    {
        return runDate;
    }

    public ARDISCOVERY_PRODUCT_ENUM getProduct()
    {
        return product;
    }

    public String getProductId()
    {
        return productId;
    }

    public String getName()
    {
        return name;
    }

    public String getDate()
    {
        return date;
    }

    public String getFilePath()
    {
        return filePath;
    }

    public float getSize()
    {
        return size;
    }

    public Drawable getThumbnail()
    {
        return thumbnail;
    }

    public MEDIA_TYPE_ENUM getMediaType()
    {
        return mediaType;
    }

    public String getUUID()
    {
        return this.uuid;
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
            model.uuid = uuid;
            model.media = this.media;
        }
        catch (CloneNotSupportedException e)
        {
            e.printStackTrace();
        }

        return model;
    }


    public static final Parcelable.Creator<ARMediaObject> CREATOR = new Creator<ARMediaObject>()
    {
        public ARMediaObject createFromParcel(Parcel source)
        {
            ARMediaObject object = new ARMediaObject();

            object.runDate = source.readString();
            object.product = ARDISCOVERY_PRODUCT_ENUM.values()[source.readInt()];
            object.name = source.readString();
            object.date = source.readString();
            object.filePath = source.readString();
            object.size = source.readFloat();
            object.thumbnail = null;
            object.mediaType = MEDIA_TYPE_ENUM.values()[source.readInt()];
            object.uuid = source.readString();
            object.media = source.readParcelable(null);
            return object;
        }

        @Override
        public ARMediaObject[] newArray(int size)
        {
            // TODO Auto-generated method stub
            return new ARMediaObject[size];
        }
    };

    @Override
    public int describeContents()
    {
        // TODO Auto-generated method stub
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags)
    {
        // TODO Auto-generated method stub
        dest.writeString(runDate);
        dest.writeInt(product.ordinal());
        dest.writeString(name);
        dest.writeString(date);
        dest.writeString(filePath);
        dest.writeFloat(size);
        dest.writeInt(mediaType.ordinal());
        dest.writeString(uuid);
        dest.writeParcelable(this.media, flags);
    }

    public int compareTo(ARMediaObject obj)
    {
        if (dateFormater == null)
        {
            dateFormater = new SimpleDateFormat("yyyy-MM-dd'T'HHmmss");
        }

        int retVal = 0;
        try
        {
            Date dateObj1 = dateFormater.parse(obj.date);
            Date dateObj2 = dateFormater.parse(this.date);
            if (dateObj1.after(dateObj2))
            {
                retVal = -1;
            }
            else if (dateObj1.before(dateObj2))
            {
                retVal = 1;
            }
        }
        catch (ParseException e)
        {
            e.printStackTrace();
        }

        return retVal;
    }

    private void readObject(ObjectInputStream aInputStream) throws ClassNotFoundException, IOException
    {
        runDate = aInputStream.readUTF();
        product = ARDISCOVERY_PRODUCT_ENUM.values()[aInputStream.readInt()];
        productId = aInputStream.readUTF();
        name = aInputStream.readUTF();
        date = aInputStream.readUTF();
        filePath = aInputStream.readUTF();
        uuid = aInputStream.readUTF();
        size = aInputStream.readFloat();
        thumbnail = null;
        mediaType = MEDIA_TYPE_ENUM.values()[aInputStream.readInt()];
    }

    /**
     * This is the default implementation of writeObject.
     * Customise if necessary.
     */
    private void writeObject(ObjectOutputStream aOutputStream) throws IOException
    {
        if (runDate != null)

        {
            aOutputStream.writeUTF(runDate);
        }
        else
        {
            aOutputStream.writeUTF("");

        }

        aOutputStream.writeInt(product.ordinal());

        if (productId != null)
        {
            aOutputStream.writeUTF(productId);
        }
        else
        {
            aOutputStream.writeUTF("");
        }

        if (name != null)
        {
            aOutputStream.writeUTF(name);
        }
        else
        {
            aOutputStream.writeUTF("");
        }

        if (date != null)
        {
            aOutputStream.writeUTF(date);
        }
        else
        {
            aOutputStream.writeUTF("");
        }

        if (filePath != null)
        {
            aOutputStream.writeUTF(filePath);
        }
        else
        {
            aOutputStream.writeUTF("");
        }

        if (uuid != null)
        {
            aOutputStream.writeUTF(uuid);
        }
        else
        {
            aOutputStream.writeUTF("");
        }

        aOutputStream.writeFloat(size);
        aOutputStream.writeInt(mediaType.ordinal());
    }
}

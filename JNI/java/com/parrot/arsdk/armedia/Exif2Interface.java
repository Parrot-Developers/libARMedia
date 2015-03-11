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

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.Integer;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.List;
import java.lang.ClassCastException;

import org.apache.sanselan.ImageReadException;
import org.apache.sanselan.Sanselan;
import org.apache.sanselan.SanselanException;
import org.apache.sanselan.common.BinaryOutputStream;
import org.apache.sanselan.formats.jpeg.JpegImageMetadata;
import org.apache.sanselan.formats.jpeg.exifRewrite.ExifRewriter;
import org.apache.sanselan.formats.tiff.TiffField;
import org.apache.sanselan.formats.tiff.TiffImageMetadata;
import org.apache.sanselan.formats.tiff.constants.ExifTagConstants;
import org.apache.sanselan.formats.tiff.constants.TagInfo;
import org.apache.sanselan.formats.tiff.constants.TiffFieldTypeConstants;
import org.apache.sanselan.formats.tiff.fieldtypes.FieldType;
import org.apache.sanselan.formats.tiff.write.TiffOutputDirectory;
import org.apache.sanselan.formats.tiff.write.TiffOutputField;
import org.apache.sanselan.formats.tiff.write.TiffOutputSet;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.util.Log;

/**
 * This class has similar implementation to Androids ExifInterface but
 * implements some additional tags that are not available through original
 * interface.
 */
public class Exif2Interface
{
    // Constants used for the Orientation Exif tag.
    // Taken from ExifInterface.java in AOSP
    public static final int ORIENTATION_UNDEFINED = 0;
    public static final int ORIENTATION_NORMAL = 1;
    public static final int ORIENTATION_FLIP_HORIZONTAL = 2;  // left right reversed mirror
    public static final int ORIENTATION_ROTATE_180 = 3;
    public static final int ORIENTATION_FLIP_VERTICAL = 4;  // upside down mirror
    public static final int ORIENTATION_TRANSPOSE = 5;  // flipped about top-left <--> bottom-right axis
    public static final int ORIENTATION_ROTATE_90 = 6;  // rotate 90 cw to right it
    public static final int ORIENTATION_TRANSVERSE = 7;  // flipped about top-right <--> bottom-left axis
    public static final int ORIENTATION_ROTATE_270 = 8;  // rotate 270 to right it

    public enum Tag {
        /** Type is String */
        IMAGE_DESCRIPTION ("ImageDescription", ExifTagConstants.EXIF_TAG_IMAGE_DESCRIPTION, TiffFieldTypeConstants.FIELD_TYPE_ASCII),
        
        /** Type is String */
        MAKE("Make", ExifTagConstants.EXIF_TAG_MAKE, TiffFieldTypeConstants.FIELD_TYPE_ASCII),
        
        ORIENTATION("Orientation", ExifTagConstants.EXIF_TAG_ORIENTATION, TiffFieldTypeConstants.FIELD_TYPE_SHORT);
        
        private final String mTagName;
        private final TagInfo mTagInfo;
        private final FieldType mFieldType;

        Tag(String name, TagInfo tag, FieldType fieldType)
        {
            mTagName = name;
            mTagInfo = tag;
            mFieldType = fieldType;
        }
        
        public String toString()
        {
            return mTagName;
        }
    }

    private static final String TAG = Exif2Interface.class.getSimpleName();
    
    private String mFilename;
    private Map<Tag, String> mAttributes;
    private byte[] mArray;
    
 
    public Exif2Interface(String filename) throws IOException
    {
        this.mFilename = filename;
        this.mArray = null;
        mAttributes = new HashMap<Tag, String>();
        loadAttributes();
    }

    public Exif2Interface(byte array[]) throws IOException
    {
        this.mFilename = null;
        this.mArray = array;
        mAttributes = new HashMap<Tag, String>();
        loadAttributes();
    }

     
    private InputStream openInputStream() throws IOException
    {
        return onOpenStream();
    }

    
    protected InputStream onOpenStream() throws IOException
    {
        InputStream  is = null;
        
        if (mFilename != null)
        {
            if (mFilename.startsWith("http")) {
                URLConnection connection = null;
                URL url = new URL(mFilename);
                connection = url.openConnection();
                is = new BufferedInputStream(connection.getInputStream());
            } else {
                File srcFile = new File(mFilename);
                is = new BufferedInputStream(new FileInputStream(srcFile));
            }
        }
        else if (mArray != null)
        {
            is = new ByteArrayInputStream(mArray);
        }
        
        return is;
    }

    /**
     * Load a set of tags from the metadata of the jpg image.
     * @throws IOException
     */
    private synchronized void loadAttributes() throws IOException
    {
        JpegImageMetadata metadata;
        InputStream is = null;
        try {
            is = openInputStream();
            metadata = (JpegImageMetadata) Sanselan.getMetadata(is, null);
        } catch (ImageReadException e1) {
            e1.printStackTrace();
            throw new IOException(e1.getMessage());
        } finally {
            if (is != null) {
                is.close();
            }
        }

        if (metadata != null) {
            Tag[] supportedTags = Tag.values();
            int size = supportedTags.length;

            for (int i = 0; i < size; ++i) {
                Tag tag = supportedTags[i];
                TiffField field = metadata.findEXIFValue(tag.mTagInfo);
                if (field != null) {
                    try {
                        String value = null;

                        if (tag.mFieldType == TiffFieldTypeConstants.FIELD_TYPE_SHORT)
                        {
                            try {
                                value = String.valueOf(field.getIntValue());
                            }
                            catch (ClassCastException integerException) {
                                try {
                                    // if the numeric value wasn't stored as a simple Integer, it
                                    // might has been saved at the first index of an array.
                                    value = String.valueOf(field.getIntArrayValue()[0]);
                                }
                                catch (ClassCastException arrayException) {}
                            }
                        }
                        else if (tag.mFieldType == TiffFieldTypeConstants.FIELD_TYPE_ASCII) {
                            value = field.getStringValue();
                        }
                        if (value != null)
                            mAttributes.put(tag, value);
                    } catch (ImageReadException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
    }


    /**
     * Returns the value of the specified tag or {@code null} if there
     * is no such tag in the JPEG file.
     *
     * @param tag the name of the tag.
     */
    public String getAttribute(Tag tag)
    {        
        return mAttributes.get(tag);
    }

    
    /**
     * Set the value of the specified tag.
     *
     * @param tag the name of the tag.
     * @param value the value of the tag.
     */    
    public void setAttribute(Tag tag, String value)
    {
        mAttributes.put(tag, value);       
    }

    
    /**
     * Save the tag data into the JPEG file. This is expensive because it involves
     * copying all the JPG data from one file to another and deleting the old file
     * and renaming the other. It's best to use {@link #setAttribute(String,String)}
     * to set all attributes to write and make a single call rather than multiple
     * calls for each attribute.
     */
    public synchronized void saveAttributes() throws IOException
    {
        if (mFilename == null) {
            return;
        }

        File srcFile = new File(mFilename);
        OutputStream os = null;
        
        try {
          TiffOutputSet outputSet = null;
          
          JpegImageMetadata metadata = (JpegImageMetadata) Sanselan.getMetadata(srcFile);
          if (metadata != null) {
              TiffImageMetadata exif = metadata.getExif();
              if (exif != null) {
                  outputSet = exif.getOutputSet();
              }
          }
          
          if (outputSet == null) {
              outputSet = new TiffOutputSet();
          }

          if (mAttributes.size() > 0) {
              TiffOutputDirectory rootDirectory = outputSet.getRootDirectory();
              Set<Tag> keys = mAttributes.keySet();
              for (Tag key : keys) {

                  TiffOutputField field = outputSet.findField(key.mTagInfo);
                  if (field != null) {
                      outputSet.removeField(key.mTagInfo);
                  }

                  String data = mAttributes.get(key);
                  byte[] bytes = null;
                  if (key.mFieldType == TiffFieldTypeConstants.FIELD_TYPE_SHORT) {
                      bytes = key.mTagInfo.encodeValue(key.mFieldType, Integer.parseInt(data), outputSet.byteOrder);
                  } else if (key.mFieldType == TiffFieldTypeConstants.FIELD_TYPE_ASCII) {
                      bytes = key.mTagInfo.encodeValue(key.mFieldType, data, outputSet.byteOrder);
                  }

                  if (bytes != null) {
                      TiffOutputField outputField = new TiffOutputField(key.mTagInfo, key.mFieldType, bytes.length, bytes);
                      rootDirectory.removeField(key.mTagInfo);
                      rootDirectory.add(outputField);
                  }
              }
              File metadataFile = File.createTempFile("exif", null, srcFile.getParentFile());
              metadataFile.createNewFile();

              os = new BufferedOutputStream(new FileOutputStream(metadataFile));
              new ExifRewriter().updateExifMetadataLossless(srcFile, os, outputSet);
              os.close();
              os = null;

              srcFile.delete();
              metadataFile.renameTo(srcFile);
          }
        } catch (SanselanException e) {
            e.printStackTrace();
            throw new IOException();
        } catch (Exception e) {
            e.printStackTrace();
        }
        finally {
            if (os != null) {
                os.close();
            }
        }
    }

    private static Bitmap rotate(Bitmap bmp, int orientation)
    {
        Matrix matrix = null;
        switch (orientation)
        {
            case ORIENTATION_UNDEFINED:
            case ORIENTATION_NORMAL:
            case ORIENTATION_FLIP_HORIZONTAL:
            case ORIENTATION_FLIP_VERTICAL:
            case ORIENTATION_TRANSPOSE:
            case ORIENTATION_TRANSVERSE:
                // do nothing
                return bmp;
            case ORIENTATION_ROTATE_90:
                matrix = new Matrix();
                matrix.postRotate(90);
                return Bitmap.createBitmap(bmp, 0, 0, bmp.getWidth(), bmp.getHeight(), matrix, true);
            case ORIENTATION_ROTATE_180:
                matrix = new Matrix();
                matrix.postRotate(180);
                return Bitmap.createBitmap(bmp, 0, 0, bmp.getWidth(), bmp.getHeight(), matrix, true);
            case ORIENTATION_ROTATE_270:
                matrix = new Matrix();
                matrix.postRotate(270);
                return Bitmap.createBitmap(bmp, 0, 0, bmp.getWidth(), bmp.getHeight(), matrix, true);
            default:
                // do nothing
                return bmp;
        }
    }

    public static Bitmap handleOrientation(Bitmap bmp, String path)
    {
        int orientation = ORIENTATION_UNDEFINED;
        try {
            Exif2Interface exif = new Exif2Interface(path);
            String orientationstr = exif.getAttribute(Exif2Interface.Tag.ORIENTATION);
            if ((orientationstr != null) && !orientationstr.equals(""))
            {
                orientation = Integer.parseInt(orientationstr);
            }
        } catch (IOException e)
        {
            e.printStackTrace();
        }

        return rotate(bmp, orientation);
    }

    public static Bitmap handleOrientation(Bitmap bmp, byte array[])
    {
        int orientation = ORIENTATION_UNDEFINED;
        try {
            Exif2Interface exif = new Exif2Interface(array);
            String orientationstr = exif.getAttribute(Exif2Interface.Tag.ORIENTATION);
            if ((orientationstr != null) && !orientationstr.equals(""))
            {
                orientation = Integer.parseInt(orientationstr);
            }
        } catch (IOException e)
        {
            e.printStackTrace();
        }

        return rotate(bmp, orientation);
    }
}

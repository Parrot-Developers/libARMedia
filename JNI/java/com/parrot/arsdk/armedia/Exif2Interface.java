package com.parrot.arsdk.armedia;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.apache.sanselan.ImageReadException;
import org.apache.sanselan.Sanselan;
import org.apache.sanselan.SanselanException;
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

import android.util.Log;

/**
 * This class has similar implementation to Androids ExifInterface but
 * implements some additional tags that are not available through original
 * interface.
 */
public class Exif2Interface
{
    public enum Tag {
        /** Type is String */
        IMAGE_DESCRIPTION ("ImageDescription", ExifTagConstants.EXIF_TAG_IMAGE_DESCRIPTION, TiffFieldTypeConstants.FIELD_TYPE_ASCII),
        
        /** Type is String */
        MAKE("Make", ExifTagConstants.EXIF_TAG_MAKE, TiffFieldTypeConstants.FIELD_TYPE_ASCII);
        
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
    
 
    public Exif2Interface(String filename) throws IOException
    {
        this.mFilename = filename;
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
        
        if (mFilename.startsWith("http")) {
            URLConnection connection = null;
            URL url = new URL(mFilename);
            connection = url.openConnection();
            is = new BufferedInputStream(connection.getInputStream());
        } else {
            File srcFile = new File(mFilename);           
            is = new BufferedInputStream(new FileInputStream(srcFile));
        }
        
        return is;
    }
    
    private synchronized void loadAttributes() throws IOException
    {
        JpegImageMetadata metadata;
        InputStream is = null;
        try {
            is = openInputStream();
            metadata = (JpegImageMetadata) Sanselan.getMetadata(is, null);
        } catch (ImageReadException e1) {
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
                        mAttributes.put(tag, field.getStringValue());
                    } catch (ImageReadException e) {
                        Log.w(TAG, "Error extracting Exif tag " + tag);
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
              Set<Tag> keys = mAttributes.keySet();
              for (Tag key:keys) {
                  String data = mAttributes.get(key);                  
                   byte[] bytes = key.mTagInfo.encodeValue(key.mFieldType, data, outputSet.byteOrder); 
                   
                   TiffOutputField outputField = new TiffOutputField(key.mTagInfo, key.mFieldType, bytes.length, bytes);                        
                   TiffOutputDirectory exifDirectory = outputSet.getOrCreateExifDirectory();
                          
                  // make sure to remove old value if present (this method will
                  // not fail if the tag does not exist).
                  exifDirectory.removeField(key.mTagInfo);
                  exifDirectory.add(outputField);
                  
                  Log.v(TAG, "Saving tag " + key + " with value " + data);
              }
              
              File outFile = File.createTempFile("exif", null, srcFile.getParentFile());
              outFile.createNewFile();
              
              os = new BufferedOutputStream(new FileOutputStream(outFile));
            
              new ExifRewriter().updateExifMetadataLossless(srcFile, os, outputSet);
            
              os.close();
              os = null;
            
              srcFile.delete();
              outFile.renameTo(srcFile);
          }          
        } catch (SanselanException e) {
            e.printStackTrace();
            throw new IOException();
        } finally {
            if (os != null) {
                os.close();
            }
        }
    }
}

//
//  ViewController.h
//  ARMediaBench
//
//  Created by Malick Sylla  on 10/10/13.
//  Copyright (c) 2013 Parrot SA. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <libARMedia/ARMedia_Manager.h>


@interface ViewController : UIViewController

@property (nonatomic, strong) NSString *pictureFolderPath;
@property (nonatomic, strong) NSString *videoFolderPath;
@property (nonatomic, strong) NSString *delosVideoFolderPath;
@property (nonatomic, strong) NSMutableDictionary *retreiveProjectDic;
@property (nonatomic, strong) IBOutlet UIButton *addMedia;
@property (nonatomic, strong) IBOutlet UIButton *addNilMedia;
@property (nonatomic, strong) IBOutlet UIButton *addVideoMedia;
@property (nonatomic, strong) IBOutlet UIButton *addVideoDelosMedia;
@property (nonatomic, strong) IBOutlet UIButton *ARMediaLibrary;
@property (nonatomic, strong) IBOutlet UIButton *updateARMediaLibrary;
@property (nonatomic, strong) IBOutlet UIButton *projectDictionary;


@end

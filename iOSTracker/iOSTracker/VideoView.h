/*
 * Copyright (c) 2012. The Regents of the University of California. All rights reserved.
 * Licensed pursuant to the terms and conditions available for viewing at:
 * http://opensource.org/licenses/BSD-3-Clause
 * and the terms and conditions of the GNU Lesser General Public License Version 2.1.
 *
 * File: VideoView.h
 * Author: Jonathan Ventura
 * Last Modified: 12.2.2012
 */

#import <UIKit/UIKit.h>


@interface VideoView : UIView {
    CGColorSpaceRef grayColorSpace;
    CGDataProviderRef dataProvider;
}
- (void)setData:(const unsigned char*)graydata;
- (void)refreshImage;
@end

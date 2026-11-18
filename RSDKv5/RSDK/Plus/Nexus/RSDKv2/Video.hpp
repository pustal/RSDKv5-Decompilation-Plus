#ifndef RSDKV2_VIDEO_H
#define RSDKV2_VIDEO_H

extern int CurrentVideoFrame;
extern int VideoFrameCount;
extern int VideoWidth;
extern int VideoHeight;
extern int VideoSurface;
extern int VideoFilePos;
extern bool VideoPlaying;

void UpdateVideoFrame();

#endif // !RSDKV2_VIDEO_H

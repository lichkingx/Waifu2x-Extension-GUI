﻿/*
    Copyright (C) 2021  Aaron Feng

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    My Github homepage: https://github.com/AaronFeng753
*/
#include "mainwindow.h"
#include "ui_mainwindow.h"
/*
直接读取视频 分辨率 然后用 自有算法 计算其应该有的比特率
单位为k
*/
int MainWindow::video_UseRes2CalculateBitrate(QString VideoFileFullPath)
{
    QMap<QString,int> res_map = video_get_Resolution(VideoFileFullPath);
    int original_height = res_map["height"];
    int original_width = res_map["width"];
    if(original_height<=0||original_width<=0)
    {
        return 0;
    }
    if(original_height<=original_width)
    {
        return original_height*6;
    }
    else
    {
        return original_width*6;
    }
}

/*
直接获取视频的分辨率
*/
QMap<QString,int> MainWindow::video_get_Resolution(QString VideoFileFullPath)
{
    QString FrameImageFullPath="";
    FrameImageFullPath = file_getBaseName(VideoFileFullPath)+"_GetVideoRes_W2xEX.jpg";
    QFile::remove(FrameImageFullPath);
    QString program = Current_Path+"/ffmpeg_waifu2xEX.exe";
    QProcess vid;
    vid.start("\""+program+"\" -ss 0 -i \""+VideoFileFullPath+"\" -vframes 1 \""+FrameImageFullPath+"\"");
    while(!vid.waitForStarted(100)&&!QProcess_stop) {}
    while(!vid.waitForFinished(100)&&!QProcess_stop) {}
    QImage qimage_original;
    qimage_original.load(FrameImageFullPath);
    int original_height = qimage_original.height();
    int original_width = qimage_original.width();
    QFile::remove(FrameImageFullPath);
    if(original_height<=0||original_width<=0)
    {
        emit Send_TextBrowser_NewMessage(tr("ERROR! Unable to read the resolution of the video. [")+VideoFileFullPath+"]");
        QMap<QString,int> empty;
        empty["height"] = 0;
        empty["width"] = 0;
        return empty;
    }
    //==============================
    QMap<QString,int> res_map;
    //读取文件信息
    res_map["height"] = original_height;
    res_map["width"] = original_width;
    return res_map;
}

/*
根据视频时长,判断是否需要分段处理
*/
bool MainWindow::video_isNeedProcessBySegment(int rowNum)
{
    if(ui->checkBox_ProcessVideoBySegment->isChecked()==false)return false;//如果没启用分段处理,直接返回false
    QString VideoFile = Table_model_video->item(rowNum,2)->text();
    if(video_get_duration(VideoFile)>ui->spinBox_SegmentDuration->value())
    {
        return true;
    }
    else
    {
        emit Send_TextBrowser_NewMessage(tr("This video is too short, so segment processing is automatically disabled.[")+VideoFile+"]");
        return false;
    }
}

/*
生成视频片段文件夹编号
*/
QString MainWindow::video_getClipsFolderNo()
{
    QString current_date =QDateTime::currentDateTime().toString("yyMMddhhmmss");
    return current_date;
}
/*
组装视频(从mp4片段组装)
*/
void MainWindow::video_AssembleVideoClips(QString VideoClipsFolderPath,QString VideoClipsFolderName,QString video_mp4_scaled_fullpath,QString AudioPath)
{
    emit Send_TextBrowser_NewMessage(tr("Start assembling video with clips:[")+video_mp4_scaled_fullpath+"]");
    //=================
    QString encoder_audio_cmd="";
    QString bitrate_audio_cmd="";
    //=======
    if(ui->checkBox_videoSettings_isEnabled->isChecked())
    {
        if(ui->lineEdit_encoder_audio->text()!="")
            encoder_audio_cmd=" -c:a "+ui->lineEdit_encoder_audio->text()+" ";
        if(ui->spinBox_bitrate_audio->value()>0)
            bitrate_audio_cmd=" -b:a "+QString::number(ui->spinBox_bitrate_audio->value(),10)+"k ";
    }
    //==============================
    QStringList VideoClips_Scan_list = file_getFileNames_in_Folder_nofilter(VideoClipsFolderPath);
    int VideoClipsNum = VideoClips_Scan_list.count();
    QStringList VideoClips_fileName_list;
    VideoClips_fileName_list.clear();
    QFileInfo vfinfo(video_mp4_scaled_fullpath);
    QString video_dir = file_getFolderPath(video_mp4_scaled_fullpath);
    /*
    生成视频片段文件完整路径QStringList
    */
    for (int VideoNameNo = 1; VideoNameNo<=VideoClipsNum; VideoNameNo++)
    {
        QString VideoClip_FullPath_tmp = VideoClipsFolderPath+"/"+QString::number(VideoNameNo,10)+".mp4";
        if(QFile::exists(VideoClip_FullPath_tmp))
        {
            VideoClips_fileName_list.append(VideoClip_FullPath_tmp);
        }
    }
    //获取一个有效的mp4片段文件完整路径
    QString Mp4Clip_forReadInfo = VideoClips_fileName_list.at(0);
    /*
    生成文件列表QString
    */
    QString FFMpegFileList_QString = "";
    for(int CurrentVideoClipNo = 0; CurrentVideoClipNo < VideoClips_fileName_list.size(); CurrentVideoClipNo++)
    {
        QString VideoClip_fullPath = VideoClips_fileName_list.at(CurrentVideoClipNo);
        QString Line = "file \'"+VideoClip_fullPath+"\'\n";
        FFMpegFileList_QString.append(Line);
    }
    //================ 将文件列表写入文件保存 ================
    QFileInfo videoFileInfo(video_mp4_scaled_fullpath);
    QString Path_FFMpegFileList = "";
    do
    {
        int random = QRandomGenerator::global()->bounded(1,1000);
        Path_FFMpegFileList = video_dir+"/"+file_getBaseName(video_mp4_scaled_fullpath)+"_fileList_"+QString::number(random,10)+"_Waifu2xEX.txt";
    }
    while(QFile::exists(Path_FFMpegFileList));
    //=========
    QFile FFMpegFileList(Path_FFMpegFileList);
    FFMpegFileList.remove();
    if (FFMpegFileList.open(QIODevice::ReadWrite | QIODevice::Text)) //QIODevice::ReadWrite支持读写
    {
        QTextStream stream(&FFMpegFileList);
        stream << FFMpegFileList_QString;
    }
    FFMpegFileList.close();
    //========
    /*
    组装视频
    */
    QString ffmpeg_path = Current_Path+"/ffmpeg_waifu2xEX.exe";
    bool Del_DenoisedAudio = false;
    //=============== 音频降噪 ========================
    if((ui->checkBox_AudioDenoise->isChecked())&&QFile::exists(AudioPath))
    {
        QString AudioPath_tmp = video_AudioDenoise(AudioPath);
        if(AudioPath_tmp!=AudioPath)
        {
            AudioPath = AudioPath_tmp;
            Del_DenoisedAudio = true;
        }
    }
    //================= 获取比特率 =================
    QString bitrate_video_cmd="";
    if(ui->spinBox_bitrate_vid->value()>0&&ui->checkBox_videoSettings_isEnabled->isChecked())
    {
        bitrate_video_cmd = " -b:v "+QString::number(ui->spinBox_bitrate_vid->value(),10)+"k ";
    }
    else
    {
        int BitRate = video_UseRes2CalculateBitrate(Mp4Clip_forReadInfo);
        if(BitRate!=0)bitrate_video_cmd = " -b:v "+QString::number(BitRate,10)+"k ";
    }
    //================= 读取视频编码器设定 ==============
    QString encoder_video_cmd="";
    if(ui->checkBox_videoSettings_isEnabled->isChecked()&&ui->lineEdit_encoder_vid->text()!="")
    {
        encoder_video_cmd = " -c:v "+ui->lineEdit_encoder_vid->text()+" ";//图像编码器
    }
    //================ 获取fps =====================
    QString fps_video_cmd="";
    QString fps = video_get_fps(Mp4Clip_forReadInfo).trimmed();
    if(fps != "0.0")
    {
        fps_video_cmd = " -r "+fps+" ";
    }
    //================= 开始处理 =============================
    QString CMD = "";
    if(QFile::exists(AudioPath))
    {
        CMD = "\""+ffmpeg_path+"\" -y -f concat -safe 0 "+fps_video_cmd+" -i \""+Path_FFMpegFileList+"\" -i \""+AudioPath+"\""+bitrate_video_cmd+encoder_video_cmd+fps_video_cmd+encoder_audio_cmd+bitrate_audio_cmd+"\""+video_mp4_scaled_fullpath+"\"";
    }
    else
    {
        CMD = "\""+ffmpeg_path+"\" -y -f concat -safe 0 "+fps_video_cmd+" -i \""+Path_FFMpegFileList+"\""+bitrate_video_cmd+encoder_video_cmd+fps_video_cmd+"\""+video_mp4_scaled_fullpath+"\"";
    }
    QProcess AssembleVideo;
    AssembleVideo.start(CMD);
    while(!AssembleVideo.waitForStarted(100)&&!QProcess_stop) {}
    while(!AssembleVideo.waitForFinished(100)&&!QProcess_stop) {}
    //检查是否发生错误
    if(!QFile::exists(video_mp4_scaled_fullpath))//检查是否成功生成视频
    {
        MultiLine_ErrorOutput_QMutex.lock();
        emit Send_TextBrowser_NewMessage(tr("Error output for FFmpeg when processing:[")+video_mp4_scaled_fullpath+"]");
        emit Send_TextBrowser_NewMessage("\n--------------------------------------");
        //标准输出
        emit Send_TextBrowser_NewMessage(AssembleVideo.readAllStandardOutput());
        //错误输出
        emit Send_TextBrowser_NewMessage(AssembleVideo.readAllStandardError());
        emit Send_TextBrowser_NewMessage("\n--------------------------------------");
        MultiLine_ErrorOutput_QMutex.unlock();
    }
    QFile::remove(Path_FFMpegFileList);//删除文件列表
    //===================
    if(Del_DenoisedAudio)QFile::remove(AudioPath);
    //==============================
    emit Send_TextBrowser_NewMessage(tr("Finish assembling video with clips:[")+video_mp4_scaled_fullpath+"]");
}
/*
将视频拆分到帧(分段的)
*/
void MainWindow::video_video2images_ProcessBySegment(QString VideoPath,QString FrameFolderPath,int StartTime,int SegmentDuration)
{
    emit Send_TextBrowser_NewMessage(tr("Start splitting video: [")+VideoPath+"]");
    //=================
    QString ffmpeg_path = Current_Path+"/ffmpeg_waifu2xEX.exe";
    QFileInfo vfinfo(VideoPath);
    QString video_dir = file_getFolderPath(vfinfo);
    QString video_filename = file_getBaseName(VideoPath);
    QString video_ext = vfinfo.suffix();
    QString video_mp4_fullpath = video_dir+"/"+video_filename+".mp4";
    //==============
    if(video_ext!="mp4")
    {
        video_mp4_fullpath = video_dir+"/"+video_filename+"_"+video_ext+".mp4";
    }
    else
    {
        video_mp4_fullpath = video_dir+"/"+video_filename+".mp4";
    }
    //=====================
    int FrameNumDigits = video_get_frameNumDigits(video_mp4_fullpath);
    QProcess video_splitFrame;
    video_splitFrame.start("\""+ffmpeg_path+"\" -y -i \""+video_mp4_fullpath+"\" -ss "+QString::number(StartTime,10)+" -t "+QString::number(SegmentDuration,10)+" \""+FrameFolderPath.replace("%","%%")+"/%0"+QString::number(FrameNumDigits,10)+"d.png\"");
    while(!video_splitFrame.waitForStarted(100)&&!QProcess_stop) {}
    while(!video_splitFrame.waitForFinished(100)&&!QProcess_stop) {}
    //============== 尝试在Win7下可能兼容的指令 ================================
    if(file_isDirEmpty(FrameFolderPath))
    {
        video_splitFrame.start("\""+ffmpeg_path+"\" -y -i \""+video_mp4_fullpath+"\" -ss "+QString::number(StartTime,10)+" -t "+QString::number(SegmentDuration,10)+" \""+FrameFolderPath.replace("%","%%")+"/%%0"+QString::number(FrameNumDigits,10)+"d.png\"");
        while(!video_splitFrame.waitForStarted(100)&&!QProcess_stop) {}
        while(!video_splitFrame.waitForFinished(100)&&!QProcess_stop) {}
    }
    //====================================
    emit Send_TextBrowser_NewMessage(tr("Finish splitting video: [")+VideoPath+"]");
}

/*
提取视频的音频
*/
void MainWindow::video_get_audio(QString VideoPath,QString AudioPath)
{
    emit Send_TextBrowser_NewMessage(tr("Extract audio from video: [")+VideoPath+"]");
    QString ffmpeg_path = Current_Path+"/ffmpeg_waifu2xEX.exe";
    QFile::remove(AudioPath);
    QProcess video_splitSound;
    video_splitSound.start("\""+ffmpeg_path+"\" -y -i \""+VideoPath+"\" \""+AudioPath+"\"");
    while(!video_splitSound.waitForStarted(100)&&!QProcess_stop) {}
    while(!video_splitSound.waitForFinished(100)&&!QProcess_stop) {}
    if(QFile::exists(AudioPath))
    {
        emit Send_TextBrowser_NewMessage(tr("Successfully extracted audio from video: [")+VideoPath+"]");
    }
    else
    {
        emit Send_TextBrowser_NewMessage(tr("Failed to extract audio from video: [")+VideoPath+tr("] This video might be a silent video, so will continue to process this video."));
    }
}
/*
将视频转换为mp4
*/
void MainWindow::video_2mp4(QString VideoPath)
{
    //=================
    QFileInfo vfinfo(VideoPath);
    QString video_ext = vfinfo.suffix();
    //==============
    if(video_ext!="mp4")
    {
        emit Send_TextBrowser_NewMessage(tr("Start converting video: [")+VideoPath+tr("] to mp4"));
        QString ffmpeg_path = Current_Path+"/ffmpeg_waifu2xEX.exe";
        QString video_dir = file_getFolderPath(vfinfo);
        QString video_filename = file_getBaseName(VideoPath);
        QString video_mp4_fullpath = video_dir+"/"+video_filename+"_"+video_ext+".mp4";
        QFile::remove(video_mp4_fullpath);
        QString vcodec_copy_cmd = "";
        QString acodec_copy_cmd = "";
        QString bitrate_vid_cmd = "";
        QString bitrate_audio_cmd = "";
        QString Extra_command = "";
        QString bitrate_OverAll = "";
        if(ui->checkBox_videoSettings_isEnabled->isChecked())
        {
            Extra_command = ui->lineEdit_ExCommand_2mp4->text().trimmed();
            if(ui->checkBox_vcodec_copy_2mp4->isChecked())
            {
                vcodec_copy_cmd = " -vcodec copy ";
            }
            else
            {
                if(ui->spinBox_bitrate_vid_2mp4->value()>0&&ui->spinBox_bitrate_audio_2mp4->value()>0)bitrate_vid_cmd = " -b:v "+QString::number(ui->spinBox_bitrate_vid_2mp4->value(),10)+"k ";
            }
            if(ui->checkBox_acodec_copy_2mp4->isChecked())
            {
                acodec_copy_cmd = " -acodec copy ";
            }
            else
            {
                if(ui->spinBox_bitrate_vid_2mp4->value()>0&&ui->spinBox_bitrate_audio_2mp4->value()>0)bitrate_audio_cmd = " -b:a "+QString::number(ui->spinBox_bitrate_audio_2mp4->value(),10)+"k ";
            }
        }
        if((ui->checkBox_videoSettings_isEnabled->isChecked()==false)||(ui->spinBox_bitrate_vid_2mp4->value()<=0||ui->spinBox_bitrate_audio_2mp4->value()<=0))
        {
            QString BitRate = video_get_bitrate(VideoPath);
            if(BitRate!="")bitrate_OverAll = " -b "+BitRate+" ";
        }
        //=====
        QProcess video_tomp4;
        video_tomp4.start("\""+ffmpeg_path+"\" -y -i \""+VideoPath+"\" "+vcodec_copy_cmd+acodec_copy_cmd+bitrate_vid_cmd+bitrate_audio_cmd+bitrate_OverAll+" "+Extra_command+" \""+video_mp4_fullpath+"\"");
        while(!video_tomp4.waitForStarted(100)&&!QProcess_stop) {}
        while(!video_tomp4.waitForFinished(100)&&!QProcess_stop) {}
        //======
        if(QFile::exists(video_mp4_fullpath))
        {
            emit Send_TextBrowser_NewMessage(tr("Successfully converted video: [")+VideoPath+tr("] to mp4"));
        }
    }
}

//===============
//获取时长(秒)
//===============
int MainWindow::video_get_duration(QString videoPath)
{
    emit Send_TextBrowser_NewMessage(tr("Get duration of the video:[")+videoPath+"]");
    //========================= 调用ffprobe读取视频信息 ======================
    QProcess *Get_Duration_process = new QProcess();
    QString cmd = "\""+Current_Path+"/ffprobe_waifu2xEX.exe\" -i \""+videoPath+"\" -v quiet -print_format ini -show_format";
    Get_Duration_process->start(cmd);
    while(!Get_Duration_process->waitForStarted(100)&&!QProcess_stop) {}
    while(!Get_Duration_process->waitForFinished(100)&&!QProcess_stop) {}
    //============= 保存ffprobe输出的ini格式文本 =============
    QString ffprobe_output_str = Get_Duration_process->readAllStandardOutput();
    //================ 将ini写入文件保存 ================
    QFileInfo videoFileInfo(videoPath);
    QString Path_video_info_ini = "";
    QString video_dir = file_getFolderPath(videoPath);
    do
    {
        int random = QRandomGenerator::global()->bounded(1,1000);
        Path_video_info_ini = video_dir+"/"+file_getBaseName(videoPath)+"_videoInfo_"+QString::number(random,10)+"_Waifu2xEX.ini";
    }
    while(QFile::exists(Path_video_info_ini));
    //=========
    QFile video_info_ini(Path_video_info_ini);
    video_info_ini.remove();
    if (video_info_ini.open(QIODevice::ReadWrite | QIODevice::Text)) //QIODevice::ReadWrite支持读写
    {
        QTextStream stream(&video_info_ini);
        stream << ffprobe_output_str;
    }
    video_info_ini.close();
    //================== 读取ini获得参数 =====================
    QSettings *configIniRead_videoInfo = new QSettings(Path_video_info_ini, QSettings::IniFormat);
    QString Duration = configIniRead_videoInfo->value("/format/duration").toString().trimmed();
    video_info_ini.remove();
    //=======================
    if(Duration=="")
    {
        emit Send_TextBrowser_NewMessage(tr("ERROR! Unable to get the duration of the [")+videoPath+tr("]."));
        return 0;
    }
    //=======================
    double Duration_double = Duration.toDouble();
    int Duration_int = (int)Duration_double;
    //=====================
    return Duration_int;
}
/*
音频降噪
*/
QString MainWindow::video_AudioDenoise(QString OriginalAudioPath)
{
    /*
    sox 输入音频.wav -n noiseprof 噪音分析.prof
    sox 输入音频.wav 输出音频.wav noisered 噪音分析.prof 0.21
    */
    emit Send_TextBrowser_NewMessage(tr("Starting to denoise audio.[")+OriginalAudioPath+"]");
    //===========
    QFileInfo fileinfo(OriginalAudioPath);
    QString file_name = file_getBaseName(OriginalAudioPath);
    QString file_ext = fileinfo.suffix();
    QString file_path = file_getFolderPath(fileinfo);
    //================
    QString program = Current_Path+"/SoX/sox_waifu2xEX.exe";
    QString DenoiseProfile = file_path+"/"+file_name+"_DenoiseProfile.dp";
    QString DenoisedAudio = file_path+"/"+file_name+"_Denoised."+file_ext;
    double DenoiseLevel = ui->doubleSpinBox_AudioDenoiseLevel->value();
    //================
    QProcess vid;
    vid.start("\""+program+"\" \""+OriginalAudioPath+"\" -n noiseprof \""+DenoiseProfile+"\"");
    while(!vid.waitForStarted(100)&&!QProcess_stop) {}
    while(!vid.waitForFinished(100)&&!QProcess_stop) {}
    //================
    vid.start("\""+program+"\" \""+OriginalAudioPath+"\" \""+DenoisedAudio+"\" noisered \""+DenoiseProfile+"\" "+QString("%1").arg(DenoiseLevel));
    while(!vid.waitForStarted(100)&&!QProcess_stop) {}
    while(!vid.waitForFinished(100)&&!QProcess_stop) {}
    //================
    if(QFile::exists(DenoisedAudio))
    {
        emit Send_TextBrowser_NewMessage(tr("Successfully denoise audio.[")+OriginalAudioPath+"]");
        QFile::remove(DenoiseProfile);
        return DenoisedAudio;
    }
    else
    {
        emit Send_TextBrowser_NewMessage(tr("Error! Unable to denoise audio.[")+OriginalAudioPath+"]");
        return OriginalAudioPath;
    }
}
/*
保存进度
*/
void MainWindow::video_write_Progress_ProcessBySegment(QString VideoConfiguration_fullPath,int StartTime,bool isSplitComplete,bool isScaleComplete,int OLDSegmentDuration)
{
    QSettings *configIniWrite = new QSettings(VideoConfiguration_fullPath, QSettings::IniFormat);
    configIniWrite->setIniCodec(QTextCodec::codecForName("UTF-8"));
    //==================== 存储进度 ==================================
    configIniWrite->setValue("/Progress/StartTime", StartTime);
    configIniWrite->setValue("/Progress/isSplitComplete", isSplitComplete);
    configIniWrite->setValue("/Progress/isScaleComplete", isScaleComplete);
    configIniWrite->setValue("/Progress/OLDSegmentDuration", OLDSegmentDuration);
}
/*
保存视频配置
*/
void MainWindow::video_write_VideoConfiguration(QString VideoConfiguration_fullPath,int ScaleRatio,int DenoiseLevel,bool CustRes_isEnabled,int CustRes_height,int CustRes_width,QString EngineName,bool isProcessBySegment,QString VideoClipsFolderPath,QString VideoClipsFolderName)
{
    QSettings *configIniWrite = new QSettings(VideoConfiguration_fullPath, QSettings::IniFormat);
    configIniWrite->setIniCodec(QTextCodec::codecForName("UTF-8"));
    //================= 添加警告 =========================
    configIniWrite->setValue("/Warning/EN", "Do not modify this file! It may cause the program to crash! If problems occur after the modification, delete this file and restart the program.");
    //==================== 存储视频信息 ==================================
    configIniWrite->setValue("/VideoConfiguration/ScaleRatio", ScaleRatio);
    configIniWrite->setValue("/VideoConfiguration/DenoiseLevel", DenoiseLevel);
    configIniWrite->setValue("/VideoConfiguration/CustRes_isEnabled", CustRes_isEnabled);
    configIniWrite->setValue("/VideoConfiguration/CustRes_height", CustRes_height);
    configIniWrite->setValue("/VideoConfiguration/CustRes_width", CustRes_width);
    configIniWrite->setValue("/VideoConfiguration/EngineName", EngineName);
    configIniWrite->setValue("/VideoConfiguration/isProcessBySegment", isProcessBySegment);
    configIniWrite->setValue("/VideoConfiguration/VideoClipsFolderPath", VideoClipsFolderPath);
    configIniWrite->setValue("/VideoConfiguration/VideoClipsFolderName", VideoClipsFolderName);
    //==================== 存储进度 ==================================
    configIniWrite->setValue("/Progress/StartTime", 0);
    configIniWrite->setValue("/Progress/isSplitComplete", false);
    configIniWrite->setValue("/Progress/isScaleComplete", false);
}

QString MainWindow::video_get_bitrate_AccordingToRes_FrameFolder(QString ScaledFrameFolderPath)
{
    QStringList flist = file_getFileNames_in_Folder_nofilter(ScaledFrameFolderPath);
    QString Full_Path_File = "";
    if(!flist.isEmpty())
    {
        for(int i = 0; i < flist.size(); i++)
        {
            QString tmp = flist.at(i);
            Full_Path_File = ScaledFrameFolderPath + "/" + tmp;
            QFileInfo finfo(Full_Path_File);
            if(finfo.suffix()=="png")break;
        }
    }
    QImage qimage_original;
    qimage_original.load(Full_Path_File);
    int original_height = qimage_original.height();
    int original_width = qimage_original.width();
    if(original_height<=0||original_width<=0)
    {
        return "";
    }
    if(original_height<=original_width)
    {
        return QString::number(original_height*6,10);
    }
    else
    {
        return QString::number(original_width*6,10);
    }
}
/*
获取视频比特率
*/
QString MainWindow::video_get_bitrate(QString videoPath)
{
    emit Send_TextBrowser_NewMessage(tr("Get bitrate of the video:[")+videoPath+"]");
    //========================= 调用ffprobe读取视频信息 ======================
    QProcess *Get_Bitrate_process = new QProcess();
    QString cmd = "\""+Current_Path+"/ffprobe_waifu2xEX.exe\" -i \""+videoPath+"\" -v quiet -print_format ini -show_format";
    Get_Bitrate_process->start(cmd);
    while(!Get_Bitrate_process->waitForStarted(100)&&!QProcess_stop) {}
    while(!Get_Bitrate_process->waitForFinished(100)&&!QProcess_stop) {}
    //============= 保存ffprobe输出的ini格式文本 =============
    QString ffprobe_output_str = Get_Bitrate_process->readAllStandardOutput();
    //================ 将ini写入文件保存 ================
    //创建文件夹
    QString video_info_dir = Current_Path+"/videoInfo";
    if(!file_isDirExist(video_info_dir))
    {
        file_mkDir(video_info_dir);
    }
    //========================
    QFileInfo videoFileInfo(videoPath);
    QString Path_video_info_ini = "";
    QString video_dir = file_getFolderPath(videoPath);
    do
    {
        int random = QRandomGenerator::global()->bounded(1,1000);
        Path_video_info_ini = video_dir+"/"+file_getBaseName(videoPath)+"_videoInfo_"+QString::number(random,10)+"_Waifu2xEX.ini";
    }
    while(QFile::exists(Path_video_info_ini));
    //=========
    QFile video_info_ini(Path_video_info_ini);
    video_info_ini.remove();
    if (video_info_ini.open(QIODevice::ReadWrite | QIODevice::Text)) //QIODevice::ReadWrite支持读写
    {
        QTextStream stream(&video_info_ini);
        stream << ffprobe_output_str;
    }
    video_info_ini.close();
    //================== 读取ini获得参数 =====================
    QSettings *configIniRead_videoInfo = new QSettings(Path_video_info_ini, QSettings::IniFormat);
    QString BitRate = configIniRead_videoInfo->value("/format/bit_rate").toString().trimmed();
    //=======================
    if(BitRate=="")emit Send_TextBrowser_NewMessage(tr("Warning! Unable to get the bitrate of the [")+videoPath+tr("]. The bit rate automatically allocated by ffmpeg will be used."));
    video_info_ini.remove();
    return BitRate;
}

QString MainWindow::video_get_fps(QString videoPath)
{
    QString program = Current_Path+"/python_ext_waifu2xEX.exe";
    QProcess vid;
    vid.start("\""+program+"\" \""+videoPath+"\" fps");
    while(!vid.waitForStarted(100)&&!QProcess_stop) {}
    while(!vid.waitForFinished(100)&&!QProcess_stop) {}
    QString fps=vid.readAllStandardOutput();
    return fps;
}

int MainWindow::video_get_frameNumDigits(QString videoPath)
{
    QString program = Current_Path+"/python_ext_waifu2xEX.exe";
    QProcess vid;
    vid.start("\""+program+"\" \""+videoPath+"\" countframe");
    while(!vid.waitForStarted(100)&&!QProcess_stop) {}
    while(!vid.waitForFinished(100)&&!QProcess_stop) {}
    int FrameNum = vid.readAllStandardOutput().toInt();
    int frameNumDigits=1+(int)log10(FrameNum);
    return frameNumDigits;
}

void MainWindow::video_video2images(QString VideoPath,QString FrameFolderPath,QString AudioPath)
{
    emit Send_TextBrowser_NewMessage(tr("Start splitting video: [")+VideoPath+"]");
    //=================
    QString ffmpeg_path = Current_Path+"/ffmpeg_waifu2xEX.exe";
    QFileInfo vfinfo(VideoPath);
    QString video_dir = file_getFolderPath(vfinfo);
    QString video_filename = file_getBaseName(VideoPath);
    QString video_ext = vfinfo.suffix();
    QString video_mp4_fullpath = video_dir+"/"+video_filename+".mp4";
    //==============
    if(video_ext!="mp4")
    {
        video_mp4_fullpath = video_dir+"/"+video_filename+"_"+video_ext+".mp4";
    }
    else
    {
        video_mp4_fullpath = video_dir+"/"+video_filename+".mp4";
    }
    //======= 转换到mp4 =======
    if(video_ext!="mp4")
    {
        QFile::remove(video_mp4_fullpath);
        QString vcodec_copy_cmd = "";
        QString acodec_copy_cmd = "";
        QString bitrate_vid_cmd = "";
        QString bitrate_audio_cmd = "";
        QString Extra_command = "";
        QString bitrate_OverAll = "";
        if(ui->checkBox_videoSettings_isEnabled->isChecked())
        {
            Extra_command = ui->lineEdit_ExCommand_2mp4->text().trimmed();
            if(ui->checkBox_vcodec_copy_2mp4->isChecked())
            {
                vcodec_copy_cmd = " -vcodec copy ";
            }
            else
            {
                if(ui->spinBox_bitrate_vid_2mp4->value()>0&&ui->spinBox_bitrate_audio_2mp4->value()>0)bitrate_vid_cmd = " -b:v "+QString::number(ui->spinBox_bitrate_vid_2mp4->value(),10)+"k ";
            }
            if(ui->checkBox_acodec_copy_2mp4->isChecked())
            {
                acodec_copy_cmd = " -acodec copy ";
            }
            else
            {
                if(ui->spinBox_bitrate_vid_2mp4->value()>0&&ui->spinBox_bitrate_audio_2mp4->value()>0)bitrate_audio_cmd = " -b:a "+QString::number(ui->spinBox_bitrate_audio_2mp4->value(),10)+"k ";
            }
        }
        if((ui->checkBox_videoSettings_isEnabled->isChecked()==false)||(ui->spinBox_bitrate_vid_2mp4->value()<=0||ui->spinBox_bitrate_audio_2mp4->value()<=0))
        {
            QString BitRate = video_get_bitrate(VideoPath);
            if(BitRate!="")bitrate_OverAll = " -b "+BitRate+" ";
        }
        //=====
        QProcess video_tomp4;
        video_tomp4.start("\""+ffmpeg_path+"\" -y -i \""+VideoPath+"\" "+vcodec_copy_cmd+acodec_copy_cmd+bitrate_vid_cmd+bitrate_audio_cmd+bitrate_OverAll+" "+Extra_command+" \""+video_mp4_fullpath+"\"");
        while(!video_tomp4.waitForStarted(100)&&!QProcess_stop) {}
        while(!video_tomp4.waitForFinished(100)&&!QProcess_stop) {}
    }
    //=====================
    int FrameNumDigits = video_get_frameNumDigits(video_mp4_fullpath);
    QProcess video_splitFrame;
    video_splitFrame.start("\""+ffmpeg_path+"\" -y -i \""+video_mp4_fullpath+"\" \""+FrameFolderPath.replace("%","%%")+"/%0"+QString::number(FrameNumDigits,10)+"d.png\"");
    while(!video_splitFrame.waitForStarted(100)&&!QProcess_stop) {}
    while(!video_splitFrame.waitForFinished(100)&&!QProcess_stop) {}
    //============== 尝试在Win7下可能兼容的指令 ================================
    if(file_isDirEmpty(FrameFolderPath))
    {
        video_splitFrame.start("\""+ffmpeg_path+"\" -y -i \""+video_mp4_fullpath+"\" \""+FrameFolderPath.replace("%","%%")+"/%%0"+QString::number(FrameNumDigits,10)+"d.png\"");
        while(!video_splitFrame.waitForStarted(100)&&!QProcess_stop) {}
        while(!video_splitFrame.waitForFinished(100)&&!QProcess_stop) {}
    }
    QFile::remove(AudioPath);
    QProcess video_splitSound;
    video_splitSound.start("\""+ffmpeg_path+"\" -y -i \""+video_mp4_fullpath+"\" \""+AudioPath+"\"");
    while(!video_splitSound.waitForStarted(100)&&!QProcess_stop) {}
    while(!video_splitSound.waitForFinished(100)&&!QProcess_stop) {}
    //====================================
    emit Send_TextBrowser_NewMessage(tr("Finish splitting video: [")+VideoPath+"]");
}

int MainWindow::video_images2video(QString VideoPath,QString video_mp4_scaled_fullpath,QString ScaledFrameFolderPath,QString AudioPath,bool CustRes_isEnabled,int CustRes_height,int CustRes_width)
{
    emit Send_TextBrowser_NewMessage(tr("Start assembling video:[")+VideoPath+"]");
    bool Del_DenoisedAudio = false;
    //=================
    QString bitrate_video_cmd="";
    //=======
    if(ui->checkBox_videoSettings_isEnabled->isChecked()&&(ui->spinBox_bitrate_vid->value()>0))
    {
        bitrate_video_cmd=" -b:v "+QString::number(ui->spinBox_bitrate_vid->value(),10)+"k ";
    }
    else
    {
        QString BitRate = video_get_bitrate_AccordingToRes_FrameFolder(ScaledFrameFolderPath);
        if(BitRate!="")bitrate_video_cmd=" -b:v "+BitRate+"k ";
    }
    //================ 自定义分辨率 ======================
    QString resize_cmd ="";
    if(CustRes_isEnabled)
    {
        //============= 如果没有自定义视频参数, 则根据自定义分辨率再计算一次比特率 ==========
        if(ui->checkBox_videoSettings_isEnabled->isChecked()==false)
        {
            int small_res =0;
            if(CustRes_width<=CustRes_height)
            {
                small_res = CustRes_width;
            }
            else
            {
                small_res = CustRes_height;
            }
            bitrate_video_cmd=" -b:v "+QString::number(small_res*6,10)+"k ";
        }
        //=================================================================
        if(CustRes_AspectRatioMode==Qt::IgnoreAspectRatio)
        {
            resize_cmd =" -vf scale="+QString::number(CustRes_width,10)+":"+QString::number(CustRes_height,10)+" ";
        }
        if(CustRes_AspectRatioMode==Qt::KeepAspectRatio)
        {
            if(CustRes_width>=CustRes_height)
            {
                resize_cmd =" -vf scale=-2:"+QString::number(CustRes_height,10)+" ";
            }
            else
            {
                resize_cmd =" -vf scale="+QString::number(CustRes_width,10)+":-2 ";
            }
        }
        if(CustRes_AspectRatioMode==Qt::KeepAspectRatioByExpanding)
        {
            if(CustRes_width>=CustRes_height)
            {
                resize_cmd =" -vf scale="+QString::number(CustRes_width,10)+":-2 ";
            }
            else
            {
                resize_cmd =" -vf scale=-2:"+QString::number(CustRes_height,10)+" ";
            }
        }
    }
    QString ffmpeg_path = Current_Path+"/ffmpeg_waifu2xEX.exe";
    int FrameNumDigits = video_get_frameNumDigits(VideoPath);
    QFileInfo vfinfo(VideoPath);
    QString video_dir = file_getFolderPath(vfinfo);
    QString video_filename = file_getBaseName(VideoPath);
    QString video_ext = vfinfo.suffix();
    //=========== 获取fps ===========
    QString fps = video_get_fps(VideoPath).trimmed();
    if(fps == "0.0")
    {
        emit Send_TextBrowser_NewMessage(tr("Error occured when processing [")+VideoPath+tr("]. Error: [Unable to get video frame rate.]"));
        return 0;
    }
    //=============== 音频降噪 ========================
    if((ui->checkBox_AudioDenoise->isChecked())&&QFile::exists(AudioPath))
    {
        QString AudioPath_tmp = video_AudioDenoise(AudioPath);
        if(AudioPath_tmp!=AudioPath)
        {
            AudioPath = AudioPath_tmp;
            Del_DenoisedAudio = true;
        }
    }
    //================= 开始处理 =============================
    QString CMD = "";
    if(QFile::exists(AudioPath))
    {
        CMD = "\""+ffmpeg_path+"\" -y -f image2 -framerate "+fps+" -r "+fps+" -i \""+ScaledFrameFolderPath.replace("%","%%")+"/%0"+QString::number(FrameNumDigits,10)+"d.png\" -i \""+AudioPath+"\" -r "+fps+bitrate_video_cmd+resize_cmd+video_ReadSettings_OutputVid(AudioPath)+" -r "+fps+" \""+video_mp4_scaled_fullpath+"\"";
    }
    else
    {
        CMD = "\""+ffmpeg_path+"\" -y -f image2 -framerate "+fps+" -r "+fps+" -i \""+ScaledFrameFolderPath.replace("%","%%")+"/%0"+QString::number(FrameNumDigits,10)+"d.png\" -r "+fps+bitrate_video_cmd+resize_cmd+video_ReadSettings_OutputVid(AudioPath)+" -r "+fps+" \""+video_mp4_scaled_fullpath+"\"";
    }
    QProcess images2video;
    images2video.start(CMD);
    while(!images2video.waitForStarted(100)&&!QProcess_stop) {}
    while(!images2video.waitForFinished(100)&&!QProcess_stop) {}
    //============== 尝试在Win7下可能兼容的指令 ================================
    if(!QFile::exists(video_mp4_scaled_fullpath))
    {
        if(QFile::exists(AudioPath))
        {
            CMD = "\""+ffmpeg_path+"\" -y -f image2 -framerate "+fps+" -r "+fps+" -i \""+ScaledFrameFolderPath.replace("%","%%")+"/%%0"+QString::number(FrameNumDigits,10)+"d.png\" -i \""+AudioPath+"\" -r "+fps+bitrate_video_cmd+resize_cmd+video_ReadSettings_OutputVid(AudioPath)+" -r "+fps+" \""+video_mp4_scaled_fullpath+"\"";
        }
        else
        {
            CMD = "\""+ffmpeg_path+"\" -y -f image2 -framerate "+fps+" -r "+fps+" -i \""+ScaledFrameFolderPath.replace("%","%%")+"/%%0"+QString::number(FrameNumDigits,10)+"d.png\" -r "+fps+bitrate_video_cmd+resize_cmd+video_ReadSettings_OutputVid(AudioPath)+" -r "+fps+" \""+video_mp4_scaled_fullpath+"\"";
        }
        QProcess images2video;
        images2video.start(CMD);
        while(!images2video.waitForStarted(100)&&!QProcess_stop) {}
        while(!images2video.waitForFinished(100)&&!QProcess_stop) {}
    }
    //===================
    if(Del_DenoisedAudio)QFile::remove(AudioPath);
    //==============================
    emit Send_TextBrowser_NewMessage(tr("Finish assembling video:[")+VideoPath+"]");
    return 0;
}

QString MainWindow::video_ReadSettings_OutputVid(QString AudioPath)
{
    QString OutputVideoSettings= " ";
    //====
    if(ui->checkBox_videoSettings_isEnabled->isChecked())
    {
        if(ui->lineEdit_encoder_vid->text()!="")
        {
            OutputVideoSettings.append("-c:v "+ui->lineEdit_encoder_vid->text()+" ");//图像编码器
        }
        //========
        if(QFile::exists(AudioPath))
        {
            if(ui->lineEdit_encoder_audio->text()!="")
            {
                OutputVideoSettings.append("-c:a "+ui->lineEdit_encoder_audio->text()+" ");//音频编码器
            }
            //=========
            if(ui->spinBox_bitrate_audio->value()>0)
            {
                OutputVideoSettings.append("-b:a "+QString::number(ui->spinBox_bitrate_audio->value(),10)+"k ");//音频比特率
            }
        }
        //=========
        if(ui->lineEdit_pixformat->text()!="")
        {
            OutputVideoSettings.append("-pix_fmt "+ui->lineEdit_pixformat->text()+" ");//pixel format
        }
        else
        {
            OutputVideoSettings.append("-pix_fmt yuv420p ");//pixel format
        }
        //===========
        if(ui->lineEdit_ExCommand_output->text()!="")
        {
            OutputVideoSettings.append(ui->lineEdit_ExCommand_output->text().trimmed()+" ");//附加指令
        }
    }
    //=========
    else
    {
        OutputVideoSettings.append("-pix_fmt yuv420p ");//pixel format
    }
    //=======
    return OutputVideoSettings;
}

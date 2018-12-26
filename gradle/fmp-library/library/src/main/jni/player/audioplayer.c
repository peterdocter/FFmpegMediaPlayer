/*
 * FFmpegMediaPlayer: A unified interface for playing audio files and streams.
 *
 * Copyright 2017 William Seemann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is a JNI example where we use native methods to play sounds
 * using OpenSL ES. See the corresponding Java source file located at:
 *
 *   src/com/example/nativeaudio/NativeAudio/NativeAudio.java
 *   https://android.googlesource.com/platform/system/media/+/gingerbread/opensles/tests/mimeUri/slesTestPlayStreamType.cpp
 */

#include <audioplayer.h>

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    VideoState *is = (VideoState *)context;

    AudioPlayer *player = &is->audio_player;

    if (player->buffer != NULL) {
        free(player->buffer);
        player->buffer = NULL;
    }
    
    int len = 4096;
    player->buffer = malloc(len);
    
    is->audio_callback(context, player->buffer, len);
    enqueue(&is->audio_player, (int16_t *) player->buffer, len);
}

// create the engine and output mix objects
void createEngine(AudioPlayer **ps)
{
    AudioPlayer *player = *ps;

    player->buffer = NULL;
    
    SLresult result;

    // create engine
    result = slCreateEngine(&player->engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // realize the engine
    result = (*player->engineObject)->Realize(player->engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the engine interface, which is needed in order to create other objects
    result = (*player->engineObject)->GetInterface(player->engineObject, SL_IID_ENGINE, &player->engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*player->engineEngine)->CreateOutputMix(player->engineEngine, &player->outputMixObject, 0, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // realize the output mix
    result = (*player->outputMixObject)->Realize(player->outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
}


// create buffer queue audio player
void createBufferQueueAudioPlayer(AudioPlayer **ps, void *state, int numChannels, int samplesPerSec, int streamType)
{
    AudioPlayer *player = *ps;

    SLuint32 channelMask = 0;
    
    if (numChannels == 2) {
        channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    } else if (numChannels == 3) {
        channelMask = SL_SPEAKER_FRONT_CENTER;
    } else {
        channelMask = SL_SPEAKER_FRONT_CENTER;
    }
    
    SLresult result;

    // configure audio source
    SLDataLocator_BufferQueue loc_bufq = {SL_DATALOCATOR_BUFFERQUEUE, BUFFER_COUNT};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, numChannels, samplesPerSec * 1000,
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        channelMask, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, player->outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids[4] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/ SL_IID_VOLUME, SL_IID_ANDROIDCONFIGURATION};
    const SLboolean req[4] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*player->engineEngine)->CreateAudioPlayer(player->engineEngine, &player->bqPlayerObject, &audioSrc, &audioSnk,
            4, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the stream type interface
    SLAndroidConfigurationItf playerConfig;
    result = (*player->bqPlayerObject)->GetInterface(player->bqPlayerObject, SL_IID_ANDROIDCONFIGURATION, (void*)&playerConfig);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // set the stream type
    //SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
    result = (*playerConfig)->SetConfiguration(playerConfig, SL_ANDROID_KEY_STREAM_TYPE, &streamType, sizeof(SLint32));
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // realize the player
    result = (*player->bqPlayerObject)->Realize(player->bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the play interface
    result = (*player->bqPlayerObject)->GetInterface(player->bqPlayerObject, SL_IID_PLAY, &player->bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the buffer queue interface
    result = (*player->bqPlayerObject)->GetInterface(player->bqPlayerObject, SL_IID_BUFFERQUEUE,
            &player->bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
    
    // register callback on the buffer queue
    result = (*player->bqPlayerBufferQueue)->RegisterCallback(player->bqPlayerBufferQueue, bqPlayerCallback, state);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the effect send interface
    result = (*player->bqPlayerObject)->GetInterface(player->bqPlayerObject, SL_IID_EFFECTSEND,
            &player->bqPlayerEffectSend);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
    // get the mute/solo interface
    result = (*player->bqPlayerObject)->GetInterface(player->bqPlayerObject, SL_IID_MUTESOLO, &player->bqPlayerMuteSolo);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
#endif

    // get the volume interface
    result = (*player->bqPlayerObject)->GetInterface(player->bqPlayerObject, SL_IID_VOLUME, &player->bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
}


// set the playing state for the buffer queue audio player
// to PLAYING (true) or PAUSED (false)
void setPlayingAudioPlayer(AudioPlayer **ps, int playstate)
{
    AudioPlayer *player = *ps;

    SLresult result;

    int state = 0;
    
    if (playstate == 0) {
        state = SL_PLAYSTATE_PLAYING;
    } else if (playstate == 1) {
        state = SL_PLAYSTATE_PAUSED;
    } else {
        state = SL_PLAYSTATE_STOPPED;
    }
    
    // make sure the URI audio player was created
    if (NULL != player->bqPlayerPlay) {

        // set the player's state
        result = (*player->bqPlayerPlay)->SetPlayState(player->bqPlayerPlay, state);
        assert(SL_RESULT_SUCCESS == result);
        (void)result;
    }
}

// expose the volume APIs
SLVolumeItf getVolume(AudioPlayer *player)
{
    return player->bqPlayerVolume;
}

void setVolumeUriAudioPlayer(AudioPlayer **ps, int millibel)
{
    AudioPlayer *player = *ps;

    
    SLresult result;
    SLVolumeItf volumeItf = getVolume(player);
    if (NULL != volumeItf) {
        //get min & max
        SLmillibel MinVolume = SL_MILLIBEL_MIN;
        SLmillibel MaxVolume = SL_MILLIBEL_MIN;
        
        (*volumeItf)->GetMaxVolumeLevel(volumeItf, &MaxVolume);
        
        SLmillibel volume = MinVolume + (SLmillibel)(((float)(MaxVolume - MinVolume)) * millibel);
        
        result = (*volumeItf)->SetVolumeLevel(volumeItf, volume);
        assert(SL_RESULT_SUCCESS == result);
        (void)result;
    }
}

void queueAudioSamples(AudioPlayer **ps, void *state)
{
    AudioPlayer *player = *ps;

    bqPlayerCallback(NULL, state);
}

int enqueue(AudioPlayer **ps, int16_t *data, int size) {
	AudioPlayer *player = *ps;

    SLresult result;
    result = (*player->bqPlayerBufferQueue)->Enqueue(player->bqPlayerBufferQueue, data, size);
    if (SL_RESULT_SUCCESS != result) {
        return 0;
    }
    
    return 0;
}

// shut down the native audio system
void shutdown(AudioPlayer **ps)
{
    AudioPlayer *player = *ps;

    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (player->bqPlayerObject != NULL) {
        (*player->bqPlayerObject)->Destroy(player->bqPlayerObject);
        player->bqPlayerObject = NULL;
        player->bqPlayerPlay = NULL;
        player->bqPlayerBufferQueue = NULL;
        player->bqPlayerEffectSend = NULL;
        player->bqPlayerMuteSolo = NULL;
        player->bqPlayerVolume = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (player->outputMixObject != NULL) {
        (*player->outputMixObject)->Destroy(player->outputMixObject);
        player->outputMixObject = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (player->engineObject != NULL) {
        (*player->engineObject)->Destroy(player->engineObject);
        player->engineObject = NULL;
        player->engineEngine = NULL;
    }
    
    // delete the audio buffer
    if (player->buffer != NULL) {
        free(player->buffer);
        player->buffer = NULL;
    }
}

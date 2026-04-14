#ifndef SOUND_H
#define SOUND_H

#include <windows.h>
#include <mmsystem.h>

// fileExistsA
// Small helper so audio loading can fall back to Debug\...
// when the current working directory is the project root.
inline bool fileExistsA(const char* path) {
    if (!path || !path[0]) return false;
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) &&
           !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// resolveAudioPath
// Converts a project-relative audio path into an absolute path.
inline void resolveAudioPath(const char* src, char* dst, size_t cap) {
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    if (!src) return;

    char tryPath[512] = { 0 };

    if (fileExistsA(src)) {
        lstrcpynA(tryPath, src, (int)sizeof(tryPath));
    } else {
        wsprintfA(tryPath, "Debug\\%s", src);
        if (!fileExistsA(tryPath)) {
            lstrcpynA(tryPath, src, (int)sizeof(tryPath));
        }
    }

    GetFullPathNameA(tryPath, (DWORD)cap, dst, NULL);
    for (char* p = dst; *p; ++p) {
        if (*p == '/') *p = '\\';
    }
}

// SFX / generic sound channel  (PlaySoundA)
// Used for menu ambience and any other simple looping/one-shot
// audio that does not need to overlap with another PlaySoundA
// clip.
inline void playSound(const char* f, bool loop = false) {
    char absPath[512] = { 0 };
    resolveAudioPath(f, absPath, sizeof(absPath));
    PlaySoundA(absPath, NULL,
               SND_FILENAME | SND_ASYNC | SND_NODEFAULT |
               (loop ? SND_LOOP : 0));
}

inline void stopSound() {
    PlaySoundA(NULL, 0, 0);
}


// Music channel
// Boss/background WAV now uses the looping PlaySoundA path.
inline void playMusic(const char* f) {
    playSound(f, true);
}

inline void stopMusic() {
    stopSound();
}


// Effect wrapper
inline void playEffect(const char* alias, const char* f) {
    if (!alias || !alias[0] || !f || !f[0]) return;

    char absPath[512] = { 0 };
    char cmd[1024] = { 0 };

    resolveAudioPath(f, absPath, sizeof(absPath));

    wsprintfA(cmd, "close %s", alias);
    mciSendStringA(cmd, NULL, 0, NULL);

    wsprintfA(cmd, "open \"%s\" type waveaudio alias %s", absPath, alias);
    mciSendStringA(cmd, NULL, 0, NULL);

    wsprintfA(cmd, "play %s from 0", alias);
    mciSendStringA(cmd, NULL, 0, NULL);
}

inline void stopEffect(const char* alias) {
    if (!alias || !alias[0]) return;

    char cmd[256] = { 0 };
    wsprintfA(cmd, "stop %s", alias);
    mciSendStringA(cmd, NULL, 0, NULL);
    wsprintfA(cmd, "close %s", alias);
    mciSendStringA(cmd, NULL, 0, NULL);
}

#endif

#!/usr/bin/env python3
"""
MJPEG 묶음 패커
입력: 폴더 안의 *.mjpeg 파일들
출력: video_pack.bin (TOC + concatenated MJPEG data)

포맷:
  [0..3]   매직 'MJPL'
  [4..7]   파일 개수 N (LE uint32)
  [8..]    엔트리 N개 × 8바이트 (offset:4 + size:4, LE)
  [...]    실제 MJPEG 데이터 연속

STM32 펌웨어가 TOC만 읽으면 N번째 비디오의 위치/크기를 바로 알 수 있어요.
"""
import os
import sys
import struct
import glob

MAGIC = b'MJPL'
FLASH_SIZE = 16 * 1024 * 1024   # MX25L12833F = 16MB

def pack(folder, output):
    files = sorted(glob.glob(os.path.join(folder, '*.mjpeg')))
    if not files:
        print(f"No .mjpeg files in {folder}")
        sys.exit(1)

    n = len(files)
    header_size = 4 + 4 + n * 8   # magic + count + (offset+size)*N
    data_offset = header_size

    entries = []
    cur = data_offset
    for f in files:
        size = os.path.getsize(f)
        entries.append((cur, size, f))
        cur += size

    with open(output, 'wb') as out:
        out.write(MAGIC)
        out.write(struct.pack('<I', n))
        for off, sz, _ in entries:
            out.write(struct.pack('<II', off, sz))
        for _, _, f in entries:
            with open(f, 'rb') as inf:
                out.write(inf.read())

        # flashrom이 chip 크기 일치를 요구하므로 0xFF로 패딩
        written = out.tell()
        if written < FLASH_SIZE:
            out.write(b'\xFF' * (FLASH_SIZE - written))

    total = os.path.getsize(output)
    print(f"\nPacked {n} files → {output}")
    print(f"Total size: {total:,} bytes ({total/1024:.1f} KB, {total/1024/1024:.2f} MB)")
    print(f"Flash usage: {total/(16*1024*1024)*100:.1f}% of 16MB\n")
    print("Index:")
    for i, (off, sz, f) in enumerate(entries):
        print(f"  [{i:2d}] @{off:>8} ({sz:>7} bytes)  {os.path.basename(f)}")

if __name__ == '__main__':
    folder = sys.argv[1] if len(sys.argv) > 1 else '.'
    output = sys.argv[2] if len(sys.argv) > 2 else 'video_pack.bin'
    pack(folder, output)

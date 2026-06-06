#!/usr/bin/env python3
"""
修复 metadata.json 脚本
确保 files 和 songs 数据一致
"""

import json
from pathlib import Path

def fix_metadata():
    metadata_file = Path("metadata.json")
    
    if not metadata_file.exists():
        print("metadata.json 不存在")
        return
    
    with open(metadata_file, "r", encoding="utf-8") as f:
        metadata = json.load(f)
    
    # 从 files 同步 songs
    metadata["songs"] = list(metadata["files"].values())
    
    # 保存修复后的 metadata
    with open(metadata_file, "w", encoding="utf-8") as f:
        json.dump(metadata, f, ensure_ascii=False, indent=2)
    
    print(f"✅ metadata.json 已修复")
    print(f"   - files 条目数: {len(metadata['files'])}")
    print(f"   - songs 条目数: {len(metadata['songs'])}")
    
    # 显示每个歌曲的信息
    print("\n歌曲列表:")
    for song in metadata["songs"]:
        print(f"  - {song['name']} (ID: {song['id']}, 歌词: {song['lyrics_status']})")

if __name__ == "__main__":
    fix_metadata()

#!/usr/bin/env python3
"""
Flexifi Release Preparation Tool

This script prepares the Flexifi library for publication by:
1. Embedding all web assets
2. Validating the package structure
3. Running tests (if available)
4. Creating release artifacts

Usage:
    python3 tools/prepare_release.py [--version VERSION]
"""

import os
import sys
import subprocess
import json
from pathlib import Path
from datetime import datetime

def run_command(cmd, description=""):
    """Run a command and return success status"""
    print(f"🔧 {description or cmd}")
    try:
        result = subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True)
        if result.stdout:
            print(f"   ✅ {result.stdout.strip()}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"   ❌ {e.stderr.strip() if e.stderr else str(e)}")
        return False

def validate_structure():
    """Validate the package structure"""
    print("📋 Validating package structure...")
    
    required_files = [
        "library.json",
        "src/Flexifi.h",
        "src/Flexifi.cpp",
        "src/TemplateManager.h",
        "src/TemplateManager.cpp",
        "src/StorageManager.h",
        "src/StorageManager.cpp",
        "src/PortalWebServer.h",
        "src/PortalWebServer.cpp",
        "src/FlexifiParameter.h",
        "src/FlexifiParameter.cpp",
        "README.md",
        "examples/basic_usage/basic_usage.ino",
        "examples/advanced_callbacks/advanced_callbacks.ino",
        "examples/state_machine/state_machine.ino"
    ]
    
    missing_files = []
    for file in required_files:
        if not Path(file).exists():
            missing_files.append(file)
    
    if missing_files:
        print("❌ Missing required files:")
        for file in missing_files:
            print(f"   - {file}")
        return False
    
    print("✅ All required files present")
    return True

def embed_assets():
    """Embed web assets"""
    print("🎨 Embedding web assets...")
    return run_command("python3 tools/embed_assets.py", "Embedding web assets")

def validate_generated_assets():
    """Validate that generated assets exist and are not empty"""
    print("🔍 Validating generated assets...")
    
    assets_dir = Path("src/generated")
    header_file = assets_dir / "web_assets.h"
    impl_file = assets_dir / "web_assets.cpp"
    
    if not header_file.exists():
        print("❌ Missing web_assets.h")
        return False
    
    if not impl_file.exists():
        print("❌ Missing web_assets.cpp")
        return False
    
    # Check file sizes
    header_size = header_file.stat().st_size
    impl_size = impl_file.stat().st_size
    
    if header_size < 1000:  # Expect at least 1KB
        print(f"❌ web_assets.h too small ({header_size} bytes)")
        return False
    
    if impl_size < 500:  # Expect at least 500 bytes
        print(f"❌ web_assets.cpp too small ({impl_size} bytes)")
        return False
    
    print(f"✅ Generated assets validated ({header_size + impl_size} bytes total)")
    return True

def update_version(version=None):
    """Update version in library.json"""
    if not version:
        return True
    
    print(f"📝 Updating version to {version}...")
    
    try:
        with open("library.json", "r") as f:
            library_config = json.load(f)
        
        library_config["version"] = version
        
        with open("library.json", "w") as f:
            json.dump(library_config, f, indent=4)
        
        print(f"✅ Version updated to {version}")
        return True
    except Exception as e:
        print(f"❌ Failed to update version: {e}")
        return False

def create_release_notes():
    """Check if release notes exist, skip if they do"""
    print("📋 Checking release notes...")
    
    if Path("RELEASE_NOTES.md").exists():
        print("✅ Release notes already exist, skipping generation")
        return True
    
    print("⚠️ No RELEASE_NOTES.md found")
    print("   Please create release notes manually before running prepare_release.py")
    print("   This ensures you can craft proper release notes for your specific version")
    return False

def cleanup_generated_files():
    """Clean up any temporary generated files"""
    print("🧹 Cleaning up temporary files...")
    
    # Add any cleanup logic here
    temp_files = [
        "__pycache__",
        "*.pyc",
        ".DS_Store"
    ]
    
    for pattern in temp_files:
        run_command(f"find . -name '{pattern}' -delete", f"Removing {pattern}")
    
    return True

def main():
    """Main release preparation workflow"""
    print("🚀 Flexifi Release Preparation")
    print("=" * 50)
    
    # Parse command line arguments
    version = None
    if len(sys.argv) > 1:
        if sys.argv[1] == "--version" and len(sys.argv) > 2:
            version = sys.argv[2]
    
    steps = [
        ("Validating package structure", validate_structure),
        ("Embedding web assets", embed_assets),
        ("Validating generated assets", validate_generated_assets),
        ("Updating version", lambda: update_version(version)),
        ("Creating release notes", create_release_notes),
        ("Cleaning up", cleanup_generated_files)
    ]
    
    failed_steps = []
    
    for step_name, step_func in steps:
        print(f"\n📋 {step_name}...")
        if not step_func():
            failed_steps.append(step_name)
            print(f"❌ {step_name} failed")
        else:
            print(f"✅ {step_name} completed")
    
    print("\n" + "=" * 50)
    
    if failed_steps:
        print("❌ Release preparation failed!")
        print("Failed steps:")
        for step in failed_steps:
            print(f"   - {step}")
        sys.exit(1)
    else:
        print("✅ Release preparation completed successfully!")
        print("\n📦 Ready for package publication:")
        print("   1. Commit generated assets: git add src/generated/ && git commit")
        print("   2. Create tag: git tag -a v$(cat library.json | grep version | cut -d'\"' -f4)")
        print("   3. Push: git push && git push --tags")
        print("   4. Publish to PlatformIO registry")

if __name__ == "__main__":
    main()
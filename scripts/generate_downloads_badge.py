#!/usr/bin/env python3
"""
Script per calcolare i download totali di TsVitch da GitHub e Homebrew Store
Può essere deployato su servizi come Vercel, Netlify Functions, o GitHub Pages
"""

import json
import requests
from datetime import datetime

def get_github_downloads():
    """Ottiene i download totali da GitHub releases"""
    try:
        response = requests.get("https://api.github.com/repos/giovannimirulla/TsVitch/releases")
        response.raise_for_status()
        releases = response.json()
        
        total_downloads = 0
        for release in releases:
            for asset in release.get('assets', []):
                total_downloads += asset.get('download_count', 0)
        
        return total_downloads
    except Exception as e:
        print(f"Errore nel recuperare i download da GitHub: {e}")
        return 0

def get_homebrew_downloads():
    """Ottiene i download da Homebrew Store"""
    try:
        response = requests.get("https://switch.cdn.fortheusers.org/repo.json")
        response.raise_for_status()
        data = response.json()
        
        tsvitch_package = None
        for package in data.get('packages', []):
            if package.get('name') == 'TsVitch':
                tsvitch_package = package
                break
        
        if tsvitch_package:
            return tsvitch_package.get('app_dls', 0)
        return 0
    except Exception as e:
        print(f"Errore nel recuperare i download da Homebrew Store: {e}")
        return 0

def format_number(num):
    """Formatta il numero con separatori di migliaia"""
    return f"{num:,}"

def generate_shield_badge(total_downloads):
    """Genera un badge in formato Shields.io"""
    formatted_downloads = format_number(total_downloads)
    
    badge_data = {
        "schemaVersion": 1,
        "label": "total downloads",
        "message": formatted_downloads,
        "color": "brightgreen"
    }
    
    return badge_data

def main():
    print("Recupero download da GitHub...")
    github_downloads = get_github_downloads()
    print(f"Download GitHub: {format_number(github_downloads)}")
    
    print("Recupero download da Homebrew Store...")
    homebrew_downloads = get_homebrew_downloads()
    print(f"Download Homebrew Store: {format_number(homebrew_downloads)}")
    
    total_downloads = github_downloads + homebrew_downloads
    print(f"Download totali: {format_number(total_downloads)}")
    
    # Genera il badge
    badge_data = generate_shield_badge(total_downloads)
    
    # Salva il badge
    with open('downloads-badge.json', 'w') as f:
        json.dump(badge_data, f, indent=2)
    
    print(f"Badge generato: downloads-badge.json")
    print(f"URL del badge: https://img.shields.io/endpoint?url=https://your-domain.com/downloads-badge.json")

if __name__ == "__main__":
    main()

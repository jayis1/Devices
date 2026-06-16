#!/usr/bin/env python3
"""
FreshKeep — Barcode Database Downloader

Downloads the Open Food Facts database for offline barcode lookups.
The database maps UPC/EAN barcodes to product names, categories, and
estimated shelf life.

Usage:
    python3 download_barcode_db.py [--limit 100000]
"""

import argparse
import csv
import json
import os
import sqlite3
import urllib.request
import gzip
import io

OPEN_FOOD_FACTS_URL = "https://static.openfoodfacts.org/data/en.openfoodfacts.org.products.csv.gz"
DB_PATH = os.path.join(os.path.dirname(__file__), "..", "software", "dashboard", "backend", "barcodes.db")

# ── Shelf life estimates by category (in days) ──────────────────────────
# These are conservative estimates for common food categories
SHELF_LIFE_BY_CATEGORY = {
    "dairy": 7,         # Milk, yogurt, cheese
    "meat": 3,           # Raw meat
    "poultry": 2,        # Raw chicken
    "fish": 2,           # Raw fish
    "produce": 5,        # Fresh fruits/vegetables
    "bread": 5,          # Bread, bakery
    "eggs": 21,          # Eggs
    "beverages": 180,    # Juice, soda
    "condiments": 365,   # Ketchup, mustard
    "frozen": 180,       # Frozen foods
    "canned": 730,       # Canned goods
    "dry": 365,          # Rice, pasta, flour
    "snacks": 90,        # Chips, crackers
    "cereals": 180,     # Breakfast cereal
    "oils": 365,        # Cooking oil
    "spices": 730,      # Spices, herbs
    "sauces": 180,      # Pasta sauce, salsa
    "deli": 7,           # Deli meat
    "prepared": 4,      # Prepared meals
    "other": 30,         # Default
}

# ── Category mapping from Open Food Facts categories ─────────────────────
CATEGORY_MAPPING = {
    "en:dairy": "dairy",
    "en:milk": "dairy",
    "en:yogurt": "dairy",
    "en:cheese": "dairy",
    "en:butter": "dairy",
    "en:meat": "meat",
    "en:beef": "meat",
    "en:pork": "meat",
    "en:chicken": "poultry",
    "en:poultry": "poultry",
    "en:fish": "fish",
    "en:seafood": "fish",
    "en:fruits": "produce",
    "en:vegetables": "produce",
    "en:produce": "produce",
    "en:bread": "bread",
    "en:bakery": "bread",
    "en:eggs": "eggs",
    "en:beverages": "beverages",
    "en:juice": "beverages",
    "en:soda": "beverages",
    "en:condiments": "condiments",
    "en:ketchup": "condiments",
    "en:mustard": "condiments",
    "en:frozen": "frozen",
    "en:canned": "canned",
    "en:soups": "canned",
    "en:dry": "dry",
    "en:pasta": "dry",
    "en:rice": "dry",
    "en:flour": "dry",
    "en:snacks": "snacks",
    "en:chips": "snacks",
    "en:cereals": "cereals",
    "en:breakfast": "cereals",
    "en:oils": "oils",
    "en:spices": "spices",
    "en:sauces": "sauces",
    "en:deli": "deli",
}


def create_database(db_path):
    """Create the SQLite barcode database."""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS products (
            barcode TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            brand TEXT,
            category TEXT NOT NULL DEFAULT 'other',
            shelf_life_days INTEGER,
            weight_grams REAL,
            image_url TEXT,
            last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    
    cursor.execute('CREATE INDEX IF NOT EXISTS idx_category ON products(category)')
    cursor.execute('CREATE INDEX IF NOT EXISTS idx_name ON products(name)')
    
    conn.commit()
    conn.close()
    print(f"Database created at {db_path}")


def download_open_food_facts(limit=None):
    """Download and parse Open Food Facts database."""
    print(f"Downloading Open Food Facts database...")
    print(f"URL: {OPEN_FOOD_FACTS_URL}")
    
    try:
        urllib.request.urlretrieve(OPEN_FOOD_FACTS_URL, "openfoodfacts.csv.gz")
        print("Download complete. Decompressing...")
    except Exception as e:
        print(f"Download failed: {e}")
        print("Using sample data instead.")
        return generate_sample_data()
    
    products = []
    with gzip.open("openfoodfacts.csv.gz", 'rt', encoding='utf-8') as f:
        reader = csv.reader(f, delimiter='\t')
        header = next(reader)
        
        # Find column indices
        barcode_idx = header.index('code') if 'code' in header else 0
        name_idx = header.index('product_name') if 'product_name' in header else 7
        brand_idx = header.index('brands') if 'brands' in header else 8
        category_idx = header.index('categories_tags') if 'categories_tags' in header else -1
        weight_idx = header.index('quantity') if 'quantity' in header else -1
        image_idx = header.index('image_url') if 'image_url' in header else -1
        
        count = 0
        for row in reader:
            if limit and count >= limit:
                break
            
            try:
                barcode = row[barcode_idx].strip() if len(row) > barcode_idx else ""
                name = row[name_idx].strip() if len(row) > name_idx else ""
                
                if not barcode or not name:
                    continue
                
                # Map category
                category = "other"
                if category_idx >= 0 and len(row) > category_idx:
                    cats = row[category_idx].split(',')
                    for cat in cats:
                        cat = cat.strip().lower()
                        if cat in CATEGORY_MAPPING:
                            category = CATEGORY_MAPPING[cat]
                            break
                
                shelf_life = SHELF_LIFE_BY_CATEGORY.get(category, 30)
                
                brand = row[brand_idx].strip() if len(row) > brand_idx else ""
                image = row[image_idx].strip() if image_idx >= 0 and len(row) > image_idx else ""
                
                products.append({
                    "barcode": barcode,
                    "name": name,
                    "brand": brand,
                    "category": category,
                    "shelf_life_days": shelf_life,
                    "image_url": image,
                })
                
                count += 1
                if count % 10000 == 0:
                    print(f"  Processed {count} products...")
            except (IndexError, ValueError):
                continue
    
    # Clean up
    os.remove("openfoodfacts.csv.gz")
    return products


def generate_sample_data():
    """Generate sample product data for testing."""
    return [
        {"barcode": "04163100067", "name": "Organic Whole Milk", "brand": "Stonyfield", "category": "dairy", "shelf_life_days": 7, "image_url": ""},
        {"barcode": "04125082103", "name": "Chicken Breast", "brand": "Perdue", "category": "poultry", "shelf_life_days": 2, "image_url": ""},
        {"barcode": "07192100368", "name": "Fresh Strawberries", "brand": "Driscoll's", "category": "produce", "shelf_life_days": 5, "image_url": ""},
        {"barcode": "07874203066", "name": "Sourdough Bread", "brand": "Canyon", "category": "bread", "shelf_life_days": 5, "image_url": ""},
        {"barcode": "04119601152", "name": "Large Eggs", "brand": "Great Value", "category": "eggs", "shelf_life_days": 21, "image_url": ""},
        {"barcode": "048000013231", "name": "Orange Juice", "brand": "Tropicana", "category": "beverages", "shelf_life_days": 14, "image_url": ""},
        {"barcode": "013000006408", "name": "Pasta Sauce", "brand": "Rao's", "category": "sauces", "shelf_life_days": 180, "image_url": ""},
        {"barcode": "070470001116", "name": "Spaghetti", "brand": "Barilla", "category": "dry", "shelf_life_days": 365, "image_url": ""},
        {"barcode": "038000138416", "name": "Kraft Mac & Cheese", "brand": "Kraft", "category": "dry", "shelf_life_days": 365, "image_url": ""},
        {"barcode": "041512001014", "name": "Cheddar Cheese", "brand": "Tillamook", "category": "dairy", "shelf_life_days": 28, "image_url": ""},
        {"barcode": "04163100082", "name": "Greek Yogurt", "brand": "Chobani", "category": "dairy", "shelf_life_days": 14, "image_url": ""},
        {"barcode": "048500003516", "name": "Avocados", "brand": "Mission", "category": "produce", "shelf_life_days": 4, "image_url": ""},
        {"barcode": "07192104556", "name": "Bananas", "brand": "Dole", "category": "produce", "shelf_life_days": 5, "image_url": ""},
        {"barcode": "014100047428", "name": "Ground Beef", "brand": "93% Lean", "category": "meat", "shelf_life_days": 2, "image_url": ""},
        {"barcode": "073230001116", "name": "Salmon Fillet", "brand": "Fresh", "category": "fish", "shelf_life_days": 2, "image_url": ""},
        {"barcode": "048000018316", "name": "Frozen Pizza", "brand": "DiGiorno", "category": "frozen", "shelf_life_days": 180, "image_url": ""},
        {"barcode": "038000133416", "name": "Can of Tomatoes", "brand": "Hunt's", "category": "canned", "shelf_life_days": 730, "image_url": ""},
        {"barcode": "040000323926", "name": "Ketchup", "brand": "Heinz", "category": "condiments", "shelf_life_days": 365, "image_url": ""},
    ]


def populate_database(products, db_path):
    """Insert products into SQLite database."""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    inserted = 0
    for product in products:
        try:
            cursor.execute('''
                INSERT OR REPLACE INTO products (barcode, name, brand, category, shelf_life_days, image_url)
                VALUES (?, ?, ?, ?, ?, ?)
            ''', (
                product["barcode"],
                product["name"],
                product.get("brand", ""),
                product.get("category", "other"),
                product.get("shelf_life_days", 30),
                product.get("image_url", ""),
            ))
            inserted += 1
        except sqlite3.Error as e:
            print(f"  Error inserting {product['barcode']}: {e}")
    
    conn.commit()
    conn.close()
    print(f"Inserted {inserted} products into database")


def lookup_barcode(barcode, db_path):
    """Look up a barcode in the database."""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    cursor.execute('SELECT * FROM products WHERE barcode = ?', (barcode,))
    result = cursor.fetchone()
    
    conn.close()
    
    if result:
        return {
            "barcode": result[0],
            "name": result[1],
            "brand": result[2],
            "category": result[3],
            "shelf_life_days": result[4],
            "image_url": result[5],
        }
    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FreshKeep Barcode Database Downloader")
    parser.add_argument("--limit", type=int, default=None,
                       help="Limit number of products to download (for testing)")
    parser.add_argument("--sample", action="store_true",
                       help="Use sample data instead of downloading")
    parser.add_argument("--lookup", type=str, default=None,
                       help="Look up a specific barcode")
    args = parser.parse_args()
    
    db_path = DB_PATH
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    
    if args.lookup:
        result = lookup_barcode(args.lookup, db_path)
        if result:
            print(json.dumps(result, indent=2))
        else:
            print(f"Barcode {args.lookup} not found")
    else:
        create_database(db_path)
        
        if args.sample:
            products = generate_sample_data()
        else:
            products = download_open_food_facts(limit=args.limit)
        
        populate_database(products, db_path)
        
        print(f"\n✅ Barcode database ready at {db_path}")
        print(f"   Total products: {len(products)}")
        print(f"   Use --lookup <barcode> to search")
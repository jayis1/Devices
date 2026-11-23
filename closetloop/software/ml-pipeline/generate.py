import json, random
random.seed(7)
rows=[]
for item in range(30):
    days=random.randint(1,30)
    rows.append({"days_since_worn": days, "repair_flag": int(random.random()<0.15), "care_prompt": int(days>14)})
with open("sample.jsonl","w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
print(f"wrote {len(rows)} synthetic smoke-test rows; not real-world data")

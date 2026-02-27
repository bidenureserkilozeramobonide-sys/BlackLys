"""
Seed the BlackLys database with realistic demo data.
Creates 5 clients, 12 missions across all statuses,
6 invoices, 4 galleries – enough to populate every page.

Usage:
  cd c:\scriptRE\backend
  python seed_realistic.py
"""
import sys, os, uuid
from datetime import datetime, timedelta

sys.path.append(os.getcwd())

from app.database import SessionLocal
from app.models import Client, Mission, MissionStatus, Invoice, InvoiceStatus, Gallery

def seed():
    db = SessionLocal()
    try:
        # Clear existing data (respecting FK order)
        print("🧹  Clearing existing data...")
        db.query(Invoice).delete()
        db.query(Gallery).delete()
        db.query(Mission).delete()
        db.query(Client).delete()
        db.commit()

        now = datetime.utcnow()

        # ── Clients ─────────────────────────────────────
        clients_data = [
            ("Agence Luxe Paris",    "Agence Luxe",        "contact@agenceluxe.paris",      "+33 1 44 55 66 77"),
            ("Immobilier Prestige",  "Prestige Group",     "info@prestige-immo.fr",         "+33 1 42 33 21 00"),
            ("Barnes International", "Barnes",             "paris@barnes-international.com", "+33 1 72 56 90 00"),
            ("Propriétés de France", "PDF Immobilier",     "contact@pdf-immo.fr",           "+33 1 53 67 88 12"),
            ("Studio Haussmann",    "SH Architecture",     "photo@studio-haussmann.fr",     "+33 6 12 34 56 78"),
        ]

        clients = []
        for name, company, email, phone in clients_data:
            c = Client(name=name, company=company, email=email, phone=phone)
            db.add(c)
            db.flush()
            clients.append(c)
            print(f"   👤  {name}")

        # ── Missions ────────────────────────────────────
        missions_data = [
            # (client_idx, title, address, pack, price, days_offset, status, notes)
            (0, "Villa Montmorency",       "12 Av. des Tilleuls, 75016",       "Pack Prestige HDR", 850,   2,   "planned",     "Grande villa, focus jardin + piscine"),
            (0, "Loft Canal St Martin",    "45 Quai de Valmy, 75010",          "Pack Standard",     450,  -1,   "processing",  "Loft industriel, briques apparentes"),
            (1, "Penthouse Étoile",        "8 Av. de la Grande Armée, 75017",  "Pack Prestige HDR", 950,  -5,   "delivered",   "Terrasse panoramique 360°"),
            (1, "Duplex Marais",           "23 Rue des Francs-Bourgeois, 75004","Pack Standard",    520,  -10,  "paid",        "Poutres apparentes, 2 niveaux"),
            (2, "Hôtel Particulier 7ème",  "14 Rue de Grenelle, 75007",        "Pack Premium",      1200, -15,  "paid",        "5 pièces, moulures, parquet Versailles"),
            (2, "Appartement Trocadéro",   "3 Place du Trocadéro, 75016",      "Pack Standard",     480,   5,   "planned",     "Vue Tour Eiffel, 3 pièces"),
            (3, "Maison Neuilly",          "56 Bd du Château, 92200 Neuilly",  "Pack Prestige HDR", 780,  -3,   "processing",  "Jardin 400m², garage double"),
            (3, "Studio Pigalle",          "18 Rue Frochot, 75009",            "Pack Essentiel",    280,  -20,  "paid",        "Petit studio design, mezzanine"),
            (3, "Loft Bastille",           "12 Rue de la Roquette, 75011",     "Pack Standard",     550,  -8,   "delivered",   "120m², verrière industrielle"),
            (4, "Château Vincennes",       "Av. de Paris, 94300 Vincennes",    "Pack Premium",      1500,  7,   "planned",     "Propriété historique, 12 pièces"),
            (4, "Atelier Belleville",      "77 Rue de Belleville, 75020",      "Pack Standard",     420,  -12,  "delivered",   "Atelier d'artiste reconverti"),
            (4, "Triplex Opéra",           "6 Bd des Capucines, 75009",        "Pack Prestige HDR", 1100, -2,   "processing",  "3 niveaux, ascenseur privatif"),
        ]

        status_map = {
            "planned": MissionStatus.planned,
            "processing": MissionStatus.processing,
            "delivered": MissionStatus.delivered,
            "paid": MissionStatus.paid,
        }

        missions = []
        for ci, title, addr, pack, price, days, status, notes in missions_data:
            m = Mission(
                client_id=clients[ci].id,
                title=title,
                address=addr,
                pack_name=pack,
                price=float(price),
                shoot_date=now + timedelta(days=days),
                status=status_map[status],
                notes=notes,
            )
            db.add(m)
            db.flush()
            missions.append(m)
            print(f"   📷  {title} [{status}]")

        # ── Invoices (for delivered + paid missions) ────
        inv_map = {
            2:  ("sent",  5),   # Penthouse Étoile
            3:  ("paid",  -5),  # Duplex Marais
            4:  ("paid",  -10), # Hôtel Particulier
            7:  ("paid",  -15), # Studio Pigalle
            8:  ("sent",  3),   # Loft Bastille
            10: ("draft", 14),  # Atelier Belleville
        }

        for idx, (inv_status, due_days) in inv_map.items():
            m = missions[idx]
            inv = Invoice(
                mission_id=m.id,
                pack_name=m.pack_name,
                amount=m.price,
                status=InvoiceStatus[inv_status],
                due_date=now + timedelta(days=due_days),
                issued_at=now - timedelta(days=abs(due_days)),
            )
            db.add(inv)
            print(f"   💶  Facture {m.title} [{inv_status}] – {m.price} €")

        # ── Galleries (for delivered/paid with content) ─
        gal_data = [
            (2,  "Penthouse Étoile – Galerie"),
            (3,  "Duplex Marais – Visite Virtuelle"),
            (4,  "Hôtel Particulier – Prestige"),
            (8,  "Loft Bastille – Portfolio"),
        ]

        for idx, title in gal_data:
            m = missions[idx]
            g = Gallery(
                mission_id=m.id,
                title=title,
                share_token=uuid.uuid4().hex[:12],
                is_public=True,
            )
            db.add(g)
            print(f"   🖼️  {title}")

        db.commit()
        print(f"\n✅  Seed terminé : {len(clients)} clients, {len(missions)} missions, "
              f"{len(inv_map)} factures, {len(gal_data)} galeries")

    except Exception as e:
        print(f"❌  Erreur : {e}")
        import traceback; traceback.print_exc()
        db.rollback()
    finally:
        db.close()


if __name__ == "__main__":
    seed()

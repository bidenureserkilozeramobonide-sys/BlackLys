from typing import List

from fastapi import APIRouter, Depends, HTTPException, status

from ... import crud, schemas
from ...database import get_db
from ...models import Client

router = APIRouter(prefix="/clients", tags=["clients"])


@router.get("/", response_model=List[schemas.ClientOut])
def list_clients(db=Depends(get_db)):
    return crud.list_clients(db)


@router.post("/", response_model=schemas.ClientOut, status_code=status.HTTP_201_CREATED)
def create_client(payload: schemas.ClientCreate, db=Depends(get_db)):
    return crud.create_client(db, payload)


def _get_or_404(db, client_id: int) -> Client:
    client = crud.get_client(db, client_id)
    if not client:
        raise HTTPException(status_code=404, detail="Client not found")
    return client


@router.get("/{client_id}", response_model=schemas.ClientOut)
def get_client(client_id: int, db=Depends(get_db)):
    return _get_or_404(db, client_id)


@router.put("/{client_id}", response_model=schemas.ClientOut)
def update_client(client_id: int, payload: schemas.ClientUpdate, db=Depends(get_db)):
    client = _get_or_404(db, client_id)
    return crud.update_client(db, client, payload)


@router.delete("/{client_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_client(client_id: int, db=Depends(get_db)):
    client = _get_or_404(db, client_id)
    crud.delete_client(db, client)
    return None


# MedSync Dashboard Frontend

Web dashboard for the MedSync medication adherence system.

Built with React + TypeScript + Vite + Tailwind CSS.

## Features

- Real-time vitals monitoring (heart rate, SpO2, activity)
- Medication schedule management
- Adherence history and streaks
- Caregiver dashboard with multi-patient view
- Alert management and configuration
- Dose verification timeline

## Setup

```bash
npm install
npm run dev
```

The frontend connects to the FastAPI backend at `http://localhost:8000`.

## API Integration

All data flows through the REST API at `/api/v1/` with real-time updates via WebSocket at `/ws/v1/live`.
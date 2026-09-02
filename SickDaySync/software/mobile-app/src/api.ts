export type Card = { title: string; body: string };

export const riskCardText = (): Card[] => [
  {
    title: 'Room spread score',
    body: 'Bedroom A is amber because CO2 is high and cough clustering is increasing.'
  },
  {
    title: 'Hydration guidance',
    body: 'Offer 250 mL of fluids in the next 30 minutes and confirm bottle weight on the Med Station.'
  },
  {
    title: 'Medication window',
    body: 'Next fever-check reminder is in 18 minutes; duplicate antipyretic lockout remains active.'
  }
];

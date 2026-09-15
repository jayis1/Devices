// WorkshopSync prototype UI — authored by jayis1.
import React from "react";
import { SafeAreaView, Text, View } from "react-native";

const cards = ["Air quality: connect sentinel", "PPE tag: awaiting local acknowledgement", "Bench: calibrate before use"];
export default function App(): React.JSX.Element {
  return <SafeAreaView><View><Text>WorkshopSync</Text><Text>Local advisory prototype — not a tool controller.</Text>{cards.map((card) => <Text key={card}>{card}</Text>)}</View></SafeAreaView>;
}

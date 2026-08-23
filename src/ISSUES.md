ISS-001 — De Encoder wordt de bron van EncoderEvent. Voor ontwikkeling kan een tijdelijke seriële testinput events in de Encoder injecteren. Hierdoor blijft de volledige productieketen intact en hoeft main.cpp geen Event-objecten te manipuleren.

ISS-002 — InputManager coalescing
Meerdere encoderpulsen kunnen worden samengevoegd voordat ze naar de JogPlanner gaan.

ISS-003 — Echte encoder testen
De tijdelijke KEY_1 → LEFT / KEY_3 → RIGHT simulatie vervangen zodra de echte encoder beschikbaar is.

ISS-004 — Async inputarchitectuur
Bepalen hoe Encoder + ButtonMatrix + InputManager onafhankelijk van de rest blijven draaien.

ISS-005 — CNCjsClient async maken
Socket.IO/reconnect/feedback mag de bediening nooit blokkeren.

ISS-006 — Data-uitwisseling tussen tasks
Waarschijnlijk queues/state snapshots/andere veilige mechanismen bepalen.

ISS-007 — JogPlanner async maken
Bijvoorbeeld een eigen vaste plannerfrequentie, onafhankelijk van de inputfrequentie.

ISS-008 — Jog cancel afronden
De bestaande CNCjs jogCancel netjes in de uiteindelijke keten opnemen.

ISS-009 — Sequence diagrams/documentatie bijwerken
Nu pas, want we weten inmiddels veel beter hoe de architectuur werkelijk geworden is.
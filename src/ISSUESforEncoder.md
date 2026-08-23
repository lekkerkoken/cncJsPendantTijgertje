ISS-003 — Encoder interrupt validation

Doel: valideren dat de nieuwe interrupt-driven encoder onder alle omstandigheden correct werkt.

Testen:

langzaam rechtsom
langzaam linksom
snel rechtsom
snel linksom
rechts ↔ links afwisselen
heel snel draaien
stilstaande encoder
eventueel langdurig draaien
controleren dat geen stappen "lekken" of dubbel geteld worden

En vooral controleren of:

fysieke encoderstappen
        ↓
Encoder
        ↓
correcte ±N
        ↓
InputManager
        ↓
correcte ±N

blijft kloppen.

ISS-004 — FreeRTOS scheduling / core allocation

Die zou ik zelfs nog later doen.

Dan onderzoeken we pas:

welke task op welke core draait;
prioriteiten;
task stack sizes;
CPU-belasting;
WiFi/CNCjs invloed;
eventuele latency;
of core-pinning überhaupt nodig is.

Dat voorkomt dat we nu optimaliseren voordat we weten of er een probleem is.

Dus voor nu zou ik zeggen:

ISS-002 implementeren → basisfunctionaliteit testen → ISS-003 openen voor de grondige encoder-validatie.

En dat is ook een mooie ontwikkelstrategie: eerst architectuur, dan gedrag valideren, daarna pas optimaliseren.
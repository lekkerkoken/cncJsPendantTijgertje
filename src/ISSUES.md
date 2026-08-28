ISS-006 — Pendant timing en concurrency onderzoeken
Doel

De pendant moet betrouwbaar en voorspelbaar reageren op input en jog-opdrachten.

Op dit moment is joggen nog steeds niet betrouwbaar. Dezelfde gebruikersactie levert niet altijd hetzelfde gedrag op. Dit kan wijzen op timingproblemen tussen de verschillende asynchrone onderdelen van de pendant, bijvoorbeeld inputverwerking, jogplanning, CNCjs-communicatie en het ontvangen/verwerken van machine-status.

Het ontbreken van de definitieve antenne is mogelijk van invloed op de WiFi-prestaties, maar mag niet zonder onderzoek als oorzaak worden aangenomen.

Observatie

De basisfunctionaliteit werkt:

encoder-events worden ontvangen;
de pendant-controller verwerkt de events;
de JogPlanner wordt aangestuurd;
CNCjs ontvangt jog-opdrachten;
de machine beweegt.

Toch is het joggen niet betrouwbaar en/of voorspelbaar.

Dit suggereert dat het probleem mogelijk niet meer in de functionele keten zelf zit, maar in de timing en onderlinge synchronisatie van de activiteiten.

Mogelijke oorzaken

Te onderzoeken:

timing van de FreeRTOS-taken;
prioriteiten van de verschillende taken;
blocking calls;
timing van WiFi- en Socket.IO-communicatie;
timing tussen encoder-input en JogPlanner;
timing van machine-state updates;
timing van het versturen van opeenvolgende jog-opdrachten;
race conditions tussen taken;
queuegedrag en eventuele queue-overflow;
het coalescen van encoder-pulsen;
vertraging/jitter in de WiFi-verbinding;
invloed van het ontbreken van de antenne;
eventuele vertraging doordat meerdere onderdelen in dezelfde task draaien.
Onderzoek

Voordat functionaliteit verder wordt aangepast, eerst inzicht krijgen in de timing.

Voor relevante gebeurtenissen timestamps/logging toevoegen, bijvoorbeeld:

encoder-puls ontstaat;
encoder-event wordt door InputManager ontvangen;
event wordt gepubliceerd;
PendantController verwerkt het event;
JogPlanner ontvangt/verwerkt de verandering;
JogPlanner genereert een jog;
CNCjsClient verstuurt de opdracht;
CNCjs ontvangt de opdracht;
machine-state verandert;
nieuwe machine-state komt terug bij de pendant.

Daarmee moet zichtbaar worden waar de vertraging of variatie ontstaat.

Bij voorkeur worden hierbij timestamps in milliseconden gebruikt, zodat één jogactie door de volledige keten gevolgd kan worden.

Belangrijke onderzoeksvraag

Is het jogprobleem werkelijk een probleem met de joglogica, of is de joglogica op zichzelf correct maar wordt deze door timing/concurrency niet betrouwbaar uitgevoerd?

Acceptatiecriteria
De timing van de volledige input → jog → machine-state keten is inzichtelijk.
Er is vastgesteld welke taken/events gelijktijdig kunnen optreden.
Eventuele race conditions of blocking calls zijn geïdentificeerd.
De invloed van WiFi/antenne is afzonderlijk getest.
Een encoderbeweging resulteert onder dezelfde omstandigheden voorspelbaar in hetzelfde joggedrag.
Pas daarna wordt de JogPlanner of andere functionele logica verder aangepast als dat daadwerkelijk nodig blijkt.
Relatie met ISS-005d

ISS-005d — Jogging correct en voorspelbaar maken blijft het functionele issue.

ISS-006 onderzoekt specifiek of timing, concurrency en communicatievertraging de oorz
use actix_web::{web, App, HttpResponse, HttpServer, Responder};
use serde::{Deserialize, Serialize};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

#[derive(Serialize)]
struct StatusResponse {
    status: String,
    requests_handled: usize,
}

#[derive(Deserialize)]
struct InputData {
    message: String,
}

#[derive(Serialize)]
struct DataResponse {
    status: String,
    message: String,
    processed_length: usize,
}

struct AppState {
    request_count: AtomicUsize,
}

async fn index(data: web::Data<Arc<AppState>>) -> impl Responder {
    let count = data.request_count.fetch_add(1, Ordering::SeqCst);
    let response = StatusResponse {
        status: "success".to_string(),
        requests_handled: count,
    };
    
    HttpResponse::Ok().json(response)
}

async fn process_data(data: web::Json<InputData>, state: web::Data<Arc<AppState>>) -> impl Responder {
    let count = state.request_count.fetch_add(1, Ordering::SeqCst);
    let response = DataResponse {
        status: "success".to_string(),
        message: data.message.clone(),
        processed_length: data.message.len(),
    };
    
    HttpResponse::Ok().json(response)
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    let state = Arc::new(AppState {
        request_count: AtomicUsize::new(0),
    });

    println!("Starting server at http://127.0.0.1:8080");
    
    HttpServer::new(move || {
        App::new()
            .app_data(web::Data::new(state.clone()))
            .route("/", web::get().to(index))
            .route("/data", web::post().to(process_data))
    })
    .workers(4)  // Number of worker threads
    .bind("127.0.0.1:8080")?
    .run()
    .await
}